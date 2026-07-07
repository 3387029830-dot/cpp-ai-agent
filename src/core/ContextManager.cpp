#include "core/ContextManager.h"

#include <algorithm>

namespace cpp_ai_agent::core {

ContextManager::ContextManager(std::size_t maxMessages) : maxMessages_(std::max<std::size_t>(1, maxMessages)) {}

std::vector<Message> ContextManager::buildWindow(const std::vector<Message>& messages) const {
    if (messages.size() <= maxMessages_) {
        return messages;
    }

    std::vector<Message> window;
    const bool preserveSystem = messages.front().role == Role::System && maxMessages_ > 1;
    if (preserveSystem) {
        window.push_back(messages.front());
    }

    const auto remaining = maxMessages_ - window.size();
    const auto start = messages.end() - static_cast<std::ptrdiff_t>(remaining);
    window.insert(window.end(), start, messages.end());
    return window;
}

}  // namespace cpp_ai_agent::core
