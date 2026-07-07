#include "llm/LlmParsing.h"

#include <nlohmann/json.hpp>

#include <utility>

namespace cpp_ai_agent::llm {

namespace {

std::vector<core::ToolCall> parseToolCalls(const nlohmann::json& apiMessage) {
    std::vector<core::ToolCall> toolCalls;
    if (!apiMessage.contains("tool_calls") || !apiMessage.at("tool_calls").is_array()) {
        return toolCalls;
    }

    for (const auto& rawToolCall : apiMessage.at("tool_calls")) {
        core::ToolCall toolCall;
        toolCall.id = rawToolCall.value("id", "");

        const auto function = rawToolCall.value("function", nlohmann::json::object());
        toolCall.name = function.value("name", "");
        toolCall.arguments = function.value("arguments", "{}");

        if (!toolCall.name.empty()) {
            toolCalls.push_back(std::move(toolCall));
        }
    }

    return toolCalls;
}

}  // namespace

core::Message parseAssistantMessage(const nlohmann::json& apiMessage) {
    std::string content;
    if (apiMessage.contains("content") && apiMessage.at("content").is_string()) {
        content = apiMessage.at("content").get<std::string>();
    }

    core::Message assistant;
    assistant.role = core::Role::Assistant;
    assistant.content = content;
    assistant.toolCalls = parseToolCalls(apiMessage);
    return assistant;
}

}  // namespace cpp_ai_agent::llm
