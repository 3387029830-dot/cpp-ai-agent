#pragma once

#include "core/Message.h"

#include <cstddef>
#include <vector>

namespace cpp_ai_agent::core {

class ContextManager {
public:
    explicit ContextManager(std::size_t maxMessages);

    std::vector<Message> buildWindow(const std::vector<Message>& messages) const;

private:
    std::size_t maxMessages_ = 0;
};

}  // namespace cpp_ai_agent::core
