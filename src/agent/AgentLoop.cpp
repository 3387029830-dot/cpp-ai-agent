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

/// Truncate a long tool result to keep the context window healthy.
/// Preserves the head (first 1500 chars) and tail (last 500 chars) so the
/// model can still see the beginning and end of large file contents.
/// The model receives the truncated version; the UI displays the full text
/// via the un-truncated AgentEvent.
std::string truncateToolResult(const std::string& content, std::size_t maxLen = 2000) {
    if (content.size() <= maxLen) {
        return content;
    }

    const std::size_t headLen = 1500;
    const std::size_t tailLen = 500;
    const auto omitted = content.size() - headLen - tailLen;

    return content.substr(0, headLen) +
           "\n\n... [省略 " + std::to_string(omitted) + " 字符] ...\n\n" +
           content.substr(content.size() - tailLen);
}

}  // namespace

AgentLoop::AgentLoop(
        const llm::ILlmClient& llm,
    const tools::ToolRegistry& tools,
    const security::PermissionManager& permissions,
        storage::JsonLogger& logger,
        int maxIterations,
        AgentEventCallback onEvent,
        int maxContextTokens,
        ToolPolicy toolPolicy
)
    : llm_(llm),
      tools_(tools),
      permissions_(permissions),
      logger_(logger),
      maxIterations_(std::max(1, maxIterations)),
      onEvent_(std::move(onEvent)),
      context_(static_cast<std::size_t>(std::max(1, maxContextTokens))),
      toolPolicy_(std::move(toolPolicy)) {}

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

    if (toolPolicy_) {
        const auto denial = toolPolicy_(toolCall.name);
        if (!denial.empty()) {//被skill拦截
            toolMessage.content = denial;
            return toolMessage;
        }
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
            toolMessage.content = truncateToolResult(result.output);
        } else {
            toolMessage.content = "Tool failed: " + result.output;
        }
    } catch (const std::exception& ex) {
        toolMessage.content = std::string("Tool execution error: ") + ex.what();
    }

    return toolMessage;
}

}  // namespace cpp_ai_agent::agent
