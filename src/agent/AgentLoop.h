#pragma once

#include "core/Message.h"
#include "core/ContextManager.h"
#include "core/Session.h"
#include "llm/LlmClient.h"
#include "security/PermissionManager.h"
#include "storage/JsonLogger.h"
#include "tools/ToolRegistry.h"

#include <functional>
#include <string>
#include <vector>

namespace cpp_ai_agent::agent {

enum class AgentEventType {
    AssistantMessage,
    ToolCall,
    ToolResult,
    Warning,
};

struct AgentEvent {
    AgentEventType type = AgentEventType::AssistantMessage;
    std::string title;
    std::string detail;
};

using AgentEventCallback = std::function<void(const AgentEvent&)>;

class AgentLoop {
public:
    AgentLoop(
        const llm::ILlmClient& llm,
        const tools::ToolRegistry& tools,
        const security::PermissionManager& permissions,
        storage::JsonLogger& logger,
        int maxIterations,
        AgentEventCallback onEvent = {},
        int maxContextMessages = 40
    );

    core::Message runTurn(core::Session& session) const;

private:
    void emit(AgentEvent event) const;
    core::Message executeToolCall(const core::ToolCall& toolCall) const;

    const llm::ILlmClient& llm_;
    const tools::ToolRegistry& tools_;
    const security::PermissionManager& permissions_;
    storage::JsonLogger& logger_;
    int maxIterations_ = 10;
    AgentEventCallback onEvent_;
    core::ContextManager context_;
};

}  // namespace cpp_ai_agent::agent
