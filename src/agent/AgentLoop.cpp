#include "agent/AgentLoop.h"

#include "tools/ITool.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace cpp_ai_agent::agent {

namespace {

std::string riskLevelToString(tools::RiskLevel risk) {
    switch (risk) {
        case tools::RiskLevel::Safe:
            return "safe";
        case tools::RiskLevel::Write:
            return "write";
        case tools::RiskLevel::Dangerous:
            return "dangerous";
    }

    return "unknown";
}

std::string shortText(const std::string& value, std::size_t maxLength = 300) {
    if (value.size() <= maxLength) {
        return value;
    }

    return value.substr(0, maxLength) + "...";
}

}  // namespace

AgentLoop::AgentLoop(
    const llm::LlmClient& llm,
    const tools::ToolRegistry& tools,
    int maxIterations,
    AgentEventCallback onEvent
)
    : llm_(llm),
      tools_(tools),
      maxIterations_(std::max(1, maxIterations)),
      onEvent_(std::move(onEvent)) {}

core::Message AgentLoop::runTurn(core::Session& session) const {
    for (int iteration = 0; iteration < maxIterations_; ++iteration) {
        auto assistant = llm_.chat(session.messages(), tools_.toolsSpec());
        session.addMessage(assistant);

        if (assistant.toolCalls.empty()) {
            emit({
                AgentEventType::AssistantMessage,
                "assistant",
                assistant.content,
            });
            return assistant;
        }

        for (const auto& toolCall : assistant.toolCalls) {
            emit({
                AgentEventType::ToolCall,
                toolCall.name,
                toolCall.arguments,
            });

            auto toolMessage = executeToolCall(toolCall);
            emit({
                AgentEventType::ToolResult,
                toolCall.name,
                shortText(toolMessage.content),
            });
            session.addMessage(std::move(toolMessage));
        }
    }

    core::Message warning;
    warning.role = core::Role::Assistant;
    warning.content = "Stopped because the agent reached the maximum tool-call iterations.";
    session.addMessage(warning);
    emit({
        AgentEventType::Warning,
        "max_iterations",
        warning.content,
    });
    return warning;
}

void AgentLoop::emit(AgentEvent event) const {
    if (onEvent_) {
        onEvent_(event);
    }
}

core::Message AgentLoop::executeToolCall(const core::ToolCall& toolCall) const {
    core::Message toolMessage;
    toolMessage.role = core::Role::Tool;
    toolMessage.toolCallId = toolCall.id;

    const auto* tool = tools_.find(toolCall.name);
    if (tool == nullptr) {
        toolMessage.content = "Tool not found: " + toolCall.name;
        return toolMessage;
    }

    if (tool->risk() != tools::RiskLevel::Safe) {
        toolMessage.content =
            "Tool '" + toolCall.name + "' has risk level '" + riskLevelToString(tool->risk()) +
            "' and is not auto-executed before M4 permission control is implemented.";
        return toolMessage;
    }

    try {
        const auto args = nlohmann::json::parse(toolCall.arguments.empty() ? "{}" : toolCall.arguments);
        const auto result = tool->execute(args);
        if (result.success) {
            toolMessage.content = result.output;
        } else {
            toolMessage.content = "Tool failed: " + result.output;
        }
    } catch (const std::exception& ex) {
        toolMessage.content = std::string("Tool execution error: ") + ex.what();
    }

    return toolMessage;
}

}  // namespace cpp_ai_agent::agent
