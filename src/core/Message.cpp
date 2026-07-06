#include "core/Message.h"

namespace cpp_ai_agent::core {

std::string roleToApiString(Role role) {
    switch (role) {
        case Role::System:
            return "system";
        case Role::User:
            return "user";
        case Role::Assistant:
            return "assistant";
        case Role::Tool:
            return "tool";
    }

    return "user";
}

}  // namespace cpp_ai_agent::core

