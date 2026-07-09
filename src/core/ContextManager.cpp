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

std::vector<Message> ContextManager::buildWindow(const std::vector<Message>& messages) const {
    // Fast path: everything fits.
    std::size_t totalTokens = 0;
    for (const auto& m : messages) {
        totalTokens += estimateTokens(m);
    }
    if (totalTokens <= maxTokens_) {
        return messages;
    }

    std::vector<Message> window;
    std::size_t usedTokens = 0;

    // Always preserve the System Prompt (by convention it is the first message).
    const bool hasSystem = !messages.empty() && messages.front().role == Role::System;
    if (hasSystem) {
        window.push_back(messages.front());
        usedTokens += estimateTokens(messages.front());
    }

    // Walk backwards from the newest message, keeping as many as the budget allows.
    // This preserves the most recent conversational context.
    for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
        if (hasSystem && it == messages.rbegin() + (messages.size() - 1)) {
            continue;  // already preserved as System Prompt
        }

        const auto tokens = estimateTokens(*it);
        if (usedTokens + tokens > maxTokens_) {
            break;
        }
        usedTokens += tokens;
        // Insert after System Prompt (if any) to maintain chronological order.
        window.insert(hasSystem ? window.begin() + 1 : window.begin(), *it);
    }

    return window;
}

}  // namespace cpp_ai_agent::core
