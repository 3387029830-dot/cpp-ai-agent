#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cpp_ai_agent::core {

enum class Role {
    System,
    User,
    Assistant,
    Tool,
};

struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;
};

struct Message {
    Role role = Role::User;
    std::string content;
    std::vector<ToolCall> toolCalls;
    std::string toolCallId;
    std::int64_t timestamp = 0;
};

std::string roleToApiString(Role role);

}  // namespace cpp_ai_agent::core

