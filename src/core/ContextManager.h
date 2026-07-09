#pragma once

#include "core/Message.h"

#include <cstddef>
#include <vector>

namespace cpp_ai_agent::core {

/// Manages the conversation context window with token-aware truncation.
///
/// Instead of counting messages, buildWindow() estimates token usage and
/// keeps the most recent messages that fit within a configurable token budget.
/// System Prompt (first message with Role::System) is always preserved.
class ContextManager {
public:
    /// @param maxTokens  Token budget for the context window (input to LLM).
    ///                   Default 8000 leaves headroom for the model's reply
    ///                   on 8K–16K context-window models.
    explicit ContextManager(std::size_t maxTokens);

    /// Build a token-budgeted window from the full message history.
    /// System Prompt is always retained; older messages are dropped first.
    std::vector<Message> buildWindow(const std::vector<Message>& messages) const;

private:
    /// Rough token count for a single message (content + tool-call metadata).
    /// Uses the common heuristic: 1 token ≈ 4 English characters.
    static std::size_t estimateTokens(const Message& msg);

    std::size_t maxTokens_ = 0;
};

}  // namespace cpp_ai_agent::core
