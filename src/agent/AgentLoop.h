#pragma once

#include "core/Message.h"
#include "core/Session.h"
#include "llm/LlmClient.h"
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
        const llm::LlmClient& llm,
        const tools::ToolRegistry& tools,
        int maxIterations,
        AgentEventCallback onEvent = {}
    );

    core::Message runTurn(core::Session& session) const;

private:
    void emit(AgentEvent event) const;
    core::Message executeToolCall(const core::ToolCall& toolCall) const;

    const llm::LlmClient& llm_;
    const tools::ToolRegistry& tools_;
    int maxIterations_ = 10;
    AgentEventCallback onEvent_;
};

}  // namespace cpp_ai_agent::agent
