#include "agent/AgentLoop.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace cpp_ai_agent::agent {

namespace {

std::string shortText(const std::string& value, std::size_t maxLength = 300) {
    if (value.size() <= maxLength) {
        return value;
    }

    return value.substr(0, maxLength) + "...";
}

}  // namespace

AgentLoop::AgentLoop(
        const llm::ILlmClient& llm,
    const tools::ToolRegistry& tools,
    const security::PermissionManager& permissions,
        storage::JsonLogger& logger,
        int maxIterations,
        AgentEventCallback onEvent,
        int maxContextMessages
)
    : llm_(llm),
      tools_(tools),
      permissions_(permissions),
      logger_(logger),
      maxIterations_(std::max(1, maxIterations)),
      onEvent_(std::move(onEvent)),
      context_(static_cast<std::size_t>(std::max(1, maxContextMessages))) {}

core::Message AgentLoop::runTurn(core::Session& session) const {
    for (int iteration = 0; iteration < maxIterations_; ++iteration) {
        auto assistant = llm_.chat(context_.buildWindow(session.messages()), tools_.toolsSpec());
        session.addMessage(assistant);
        logger_.log("assistant_message", {{"content", assistant.content}});

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
            logger_.log(
                "tool_call",
                {
                    {"id", toolCall.id},
                    {"name", toolCall.name},
                    {"arguments", toolCall.arguments},
                }
            );

            auto toolMessage = executeToolCall(toolCall);
            emit({
                AgentEventType::ToolResult,
                toolCall.name,
                shortText(toolMessage.content),
            });
            logger_.log(
                "tool_result",
                {
                    {"tool_call_id", toolMessage.toolCallId},
                    {"content", toolMessage.content},
                }
            );
            session.addMessage(std::move(toolMessage));
        }
    }

    core::Message warning;
    warning.role = core::Role::Assistant;
    warning.content = "Stopped because the agent reached the maximum tool-call iterations.";
    session.addMessage(warning);
    logger_.log("warning", {{"content", warning.content}});
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

    try {
        const auto args = nlohmann::json::parse(toolCall.arguments.empty() ? "{}" : toolCall.arguments);
        const auto preview = tool->risk() == tools::RiskLevel::Safe ? "" : tool->preview(args);
        if (!permissions_.approve({toolCall.name, tool->risk(), toolCall.arguments, preview})) {
            toolMessage.content =
                "Tool '" + toolCall.name + "' with risk level '" +
                security::riskLevelToString(tool->risk()) + "' was not approved.";
            return toolMessage;
        }

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
