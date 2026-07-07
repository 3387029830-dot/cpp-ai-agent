#include <doctest/doctest.h>

#include "agent/AgentLoop.h"
#include "llm/LlmClient.h"
#include "tools/ITool.h"

#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace {

class EchoTool final : public cpp_ai_agent::tools::ITool {
public:
    std::string name() const override {
        return "echo_tool";
    }

    std::string description() const override {
        return "Echo a value for AgentLoop tests.";
    }

    nlohmann::json parametersSchema() const override {
        return {
            {"type", "object"},
            {"properties", {{"value", {{"type", "string"}}}}},
        };
    }

    cpp_ai_agent::tools::RiskLevel risk() const override {
        return cpp_ai_agent::tools::RiskLevel::Safe;
    }

    cpp_ai_agent::tools::ToolResult execute(const nlohmann::json& arguments) const override {
        return {true, arguments.value("value", ""), "text/plain"};
    }
};

class ScriptedLlm final : public cpp_ai_agent::llm::ILlmClient {
public:
    explicit ScriptedLlm(std::vector<cpp_ai_agent::core::Message> responses)
        : responses_(std::move(responses)) {}

    cpp_ai_agent::core::Message chat(const std::vector<cpp_ai_agent::core::Message>& messages) const override {
        return chat(messages, nlohmann::json::array());
    }

    cpp_ai_agent::core::Message chat(
        const std::vector<cpp_ai_agent::core::Message>& messages,
        const nlohmann::json& toolsSpec
    ) const override {
        seenMessages.push_back(messages);
        seenTools.push_back(toolsSpec);
        if (calls >= responses_.size()) {
            return assistant("done");
        }
        return responses_.at(calls++);
    }

    static cpp_ai_agent::core::Message assistant(std::string content) {
        cpp_ai_agent::core::Message message;
        message.role = cpp_ai_agent::core::Role::Assistant;
        message.content = std::move(content);
        return message;
    }

    static cpp_ai_agent::core::Message toolCall(std::string id) {
        auto message = assistant("");
        message.toolCalls.push_back({std::move(id), "echo_tool", R"({"value":"tool-output"})"});
        return message;
    }

    mutable std::size_t calls = 0;
    mutable std::vector<std::vector<cpp_ai_agent::core::Message>> seenMessages;
    mutable std::vector<nlohmann::json> seenTools;

private:
    std::vector<cpp_ai_agent::core::Message> responses_;
};

std::filesystem::path makeTempHistoryDir(const std::string& name) {
    auto root = std::filesystem::temp_directory_path() / ("cpp-ai-agent-agent-loop-" + name);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

cpp_ai_agent::core::Message userMessage(std::string content) {
    cpp_ai_agent::core::Message message;
    message.role = cpp_ai_agent::core::Role::User;
    message.content = std::move(content);
    return message;
}

cpp_ai_agent::core::Message systemMessage(std::string content) {
    cpp_ai_agent::core::Message message;
    message.role = cpp_ai_agent::core::Role::System;
    message.content = std::move(content);
    return message;
}

}  // namespace

TEST_CASE("AgentLoop dispatches tool calls and sends tool results back to the LLM") {
    const auto historyDir = makeTempHistoryDir("dispatch");
    {
        cpp_ai_agent::storage::JsonLogger logger(historyDir);
        cpp_ai_agent::tools::ToolRegistry tools;
        tools.registerTool(std::make_shared<EchoTool>());
        cpp_ai_agent::security::PermissionManager permissions(cpp_ai_agent::security::PermissionMode::ReadOnly);
        ScriptedLlm llm({ScriptedLlm::toolCall("call-1"), ScriptedLlm::assistant("final")});
        cpp_ai_agent::agent::AgentLoop loop(llm, tools, permissions, logger, 10);
        cpp_ai_agent::core::Session session("test");
        session.addMessage(userMessage("use a tool"));

        const auto reply = loop.runTurn(session);

        CHECK(reply.content == "final");
        REQUIRE(llm.calls == 2);
        REQUIRE(llm.seenMessages.size() == 2);
        const auto& secondCallMessages = llm.seenMessages.at(1);
        REQUIRE(secondCallMessages.size() >= 3);
        CHECK(secondCallMessages.back().role == cpp_ai_agent::core::Role::Tool);
        CHECK(secondCallMessages.back().toolCallId == "call-1");
        CHECK(secondCallMessages.back().content == "tool-output");
        CHECK_FALSE(llm.seenTools.at(0).empty());
    }

    std::filesystem::remove_all(historyDir);
}

TEST_CASE("AgentLoop stops after maximum tool-call iterations") {
    const auto historyDir = makeTempHistoryDir("max-iterations");
    {
        cpp_ai_agent::storage::JsonLogger logger(historyDir);
        cpp_ai_agent::tools::ToolRegistry tools;
        tools.registerTool(std::make_shared<EchoTool>());
        cpp_ai_agent::security::PermissionManager permissions(cpp_ai_agent::security::PermissionMode::ReadOnly);
        ScriptedLlm llm({ScriptedLlm::toolCall("call-1"), ScriptedLlm::toolCall("call-2")});
        cpp_ai_agent::agent::AgentLoop loop(llm, tools, permissions, logger, 2);
        cpp_ai_agent::core::Session session("test");
        session.addMessage(userMessage("loop forever"));

        const auto reply = loop.runTurn(session);

        CHECK(llm.calls == 2);
        CHECK(reply.content == "Stopped because the agent reached the maximum tool-call iterations.");
        CHECK(session.messages().back().content == reply.content);
    }

    std::filesystem::remove_all(historyDir);
}

TEST_CASE("AgentLoop sends a bounded context window to the LLM") {
    const auto historyDir = makeTempHistoryDir("context-window");
    {
        cpp_ai_agent::storage::JsonLogger logger(historyDir);
        cpp_ai_agent::tools::ToolRegistry tools;
        tools.registerTool(std::make_shared<EchoTool>());
        cpp_ai_agent::security::PermissionManager permissions(cpp_ai_agent::security::PermissionMode::ReadOnly);
        ScriptedLlm llm({ScriptedLlm::assistant("final")});
        cpp_ai_agent::agent::AgentLoop loop(llm, tools, permissions, logger, 10, {}, 3);
        cpp_ai_agent::core::Session session("test");
        session.addMessage(systemMessage("system"));
        session.addMessage(userMessage("old"));
        session.addMessage(ScriptedLlm::assistant("middle"));
        session.addMessage(userMessage("new"));

        loop.runTurn(session);

        REQUIRE(llm.seenMessages.size() == 1);
        const auto& sent = llm.seenMessages.at(0);
        REQUIRE(sent.size() == 3);
        CHECK(sent.at(0).content == "system");
        CHECK(sent.at(1).content == "middle");
        CHECK(sent.at(2).content == "new");
    }

    std::filesystem::remove_all(historyDir);
}

TEST_CASE("AgentLoop blocks tool calls denied by tool policy") {
    const auto historyDir = makeTempHistoryDir("tool-policy");
    {
        cpp_ai_agent::storage::JsonLogger logger(historyDir);
        cpp_ai_agent::tools::ToolRegistry tools;
        tools.registerTool(std::make_shared<EchoTool>());
        cpp_ai_agent::security::PermissionManager permissions(cpp_ai_agent::security::PermissionMode::ReadOnly);
        ScriptedLlm llm({ScriptedLlm::toolCall("call-1"), ScriptedLlm::assistant("final")});
        cpp_ai_agent::agent::AgentLoop loop(
            llm,
            tools,
            permissions,
            logger,
            10,
            {},
            40,
            [](const std::string& toolName) {
                return "Tool '" + toolName + "' is blocked by active skill 'project_summary'.";
            }
        );
        cpp_ai_agent::core::Session session("test");
        session.addMessage(userMessage("use a blocked tool"));

        const auto reply = loop.runTurn(session);

        CHECK(reply.content == "final");
        REQUIRE(llm.seenMessages.size() == 2);
        const auto& secondCallMessages = llm.seenMessages.at(1);
        CHECK(secondCallMessages.back().content.find("blocked by active skill") != std::string::npos);
    }

    std::filesystem::remove_all(historyDir);
}
