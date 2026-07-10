#include "core/ContextManager.h"

#include <algorithm>

namespace cpp_ai_agent::core {

ContextManager::ContextManager(std::size_t maxTokens) : maxTokens_(std::max<std::size_t>(1, maxTokens)) {}

std::size_t ContextManager::estimateTokens(const Message& msg) {
    // Count all text the message contributes to the API request body.
    // 1 token ≈ 4 English characters is the standard rough heuristic;
    // it's cheap and accurate enough for sliding-window decisions.
    std::size_t chars = msg.content.size();
    for (const auto& tc : msg.toolCalls) {
        chars += tc.id.size() + tc.name.size() + tc.arguments.size();
    }
    chars += msg.toolCallId.size();
    return std::max<std::size_t>(1, chars / 4);
}

namespace {

// A group of messages that must be kept together atomically.
// Assistant messages with tool_calls and their subsequent Tool results
// form an indivisible unit — splitting them causes "No tool output found"
// errors from the LLM API.
struct MessageGroup {
    std::vector<Message> messages;
    std::size_t tokens = 0;
};

}  // namespace

std::vector<Message> ContextManager::buildWindow(const std::vector<Message>& messages) const {
    // Fast path: everything fits.
    std::size_t totalTokens = 0;
    for (const auto& m : messages) {
        totalTokens += estimateTokens(m);
    }
    if (totalTokens <= maxTokens_) {
        return messages;
    }

    // ── Pass 1: group messages ──────────────────────────────────────
    // An Assistant message that carries tool_calls and the Tool messages
    // that follow it are treated as one atomic group.  Standalone
    // messages (User, plain Assistant, etc.) each form their own group.
    std::vector<MessageGroup> groups;
    const bool hasSystem = !messages.empty() && messages.front().role == Role::System;

    std::size_t idx = 0;
    while (idx < messages.size()) {
        const auto& current = messages[idx];

        // System Prompt — always its own group (handled specially in pass 2).
        if (idx == 0 && hasSystem) {
            MessageGroup g;
            g.messages.push_back(current);
            g.tokens += estimateTokens(current);
            groups.push_back(std::move(g));
            ++idx;
            continue;
        }

        // Assistant with tool_calls → group with all following Tool messages.
        if (current.role == Role::Assistant && !current.toolCalls.empty()) {
            MessageGroup g;
            g.messages.push_back(current);
            g.tokens += estimateTokens(current);
            ++idx;
            while (idx < messages.size() && messages[idx].role == Role::Tool) {
                g.messages.push_back(messages[idx]);
                g.tokens += estimateTokens(messages[idx]);
                ++idx;
            }
            groups.push_back(std::move(g));
        } else {
            MessageGroup g;
            g.messages.push_back(current);
            g.tokens += estimateTokens(current);
            groups.push_back(std::move(g));
            ++idx;
        }
    }

    // ── Pass 2: walk groups backwards, keep those that fit ──────────
    std::vector<Message> window;
    std::size_t usedTokens = 0;

    // System Prompt always stays.
    std::size_t startIdx = 0;
    if (hasSystem) {
        for (const auto& m : groups[0].messages) {
            window.push_back(m);
        }
        usedTokens += groups[0].tokens;
        startIdx = 1;
    }

    // Collect groups from newest to oldest while the token budget allows.
    std::vector<const MessageGroup*> keptGroups;
    for (int i = static_cast<int>(groups.size()) - 1; i >= static_cast<int>(startIdx); --i) {
        if (usedTokens + groups[static_cast<std::size_t>(i)].tokens > maxTokens_) {
            break;
        }
        usedTokens += groups[static_cast<std::size_t>(i)].tokens;
        keptGroups.push_back(&groups[static_cast<std::size_t>(i)]);
    }

    // Re-insert in chronological order (reverse of the backwards collection).
    for (auto it = keptGroups.rbegin(); it != keptGroups.rend(); ++it) {
        for (const auto& m : (*it)->messages) {
            window.push_back(m);
        }
    }

    return window;
}

}  // namespace cpp_ai_agent::core
