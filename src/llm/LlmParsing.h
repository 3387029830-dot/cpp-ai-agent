#pragma once

#include "core/Message.h"

#include <nlohmann/json_fwd.hpp>

namespace cpp_ai_agent::llm {

core::Message parseAssistantMessage(const nlohmann::json& apiMessage);

}  // namespace cpp_ai_agent::llm
