// ContextManager 测试 —— token 预算感知的上下文窗口
// =======================================================
//
// ContextManager 负责在发送 LLM 请求前截断对话历史，核心策略：
//   1. System Prompt（Role::System 的第一条消息）永驻，不参与淘汰
//   2. 按 token 估算从后往前保留消息（最新优先）
//   3. "Assistant(tool_calls) + 后续 Tool 结果" 作为原子组 ——
//      要么全保留，要么全丢弃。拆分会导致 API 报 400：
//      "No tool output found for function call"
//   4. 单个消息 token 估算公式：content / 4（≈ 1 token / 4 英文字符）
//
// 测试覆盖：
//   - 基础：未超预算全保留 / 超预算保留 System + 最新 N 条
//   - 边界 1：tool-call 组超过剩余预算 → 整组丢弃，不留孤儿 tool_call
//   - 边界 2：超大 System Prompt（如 AGENTS.md 数千字符）→ 始终保留，
//            仅剩预算分配给最新消息
//   - 边界 3：两个独立 tool-call 组被 User 消息隔开 → 各自独立截断，
//            不会跨组粘连

#include <doctest/doctest.h>
#include "core/ContextManager.h"

using cpp_ai_agent::core::ContextManager;
using cpp_ai_agent::core::Message;
using cpp_ai_agent::core::Role;

namespace {

Message message(Role role, std::string content) {
    Message result;
    result.role = role;
    result.content = std::move(content);
    return result;
}

Message assistantWithToolCalls(const std::string& content,
                               const std::vector<std::pair<std::string, std::string>>& calls) {
    Message result;
    result.role = Role::Assistant;
    result.content = content;
    for (const auto& [name, args] : calls) {
        cpp_ai_agent::core::ToolCall tc;
        tc.id = "call_" + name;
        tc.name = name;
        tc.arguments = args;
        result.toolCalls.push_back(std::move(tc));
    }
    return result;
}

Message toolResult(const std::string& toolCallId, const std::string& output) {
    Message result;
    result.role = Role::Tool;
    result.toolCallId = toolCallId;
    result.content = output;
    return result;
}

}  // namespace

TEST_CASE("ContextManager preserves system prompt and newest messages") {
    const std::vector<Message> messages = {
        message(Role::System, "system"),
        message(Role::User, "old user"),
        message(Role::Assistant, "old assistant"),
        message(Role::User, "new user"),
        message(Role::Assistant, "new assistant"),
    };

    // "system"(1) + "new user"(2) + "new assistant"(3) ≈ 6 tokens
    const auto window = ContextManager(6).buildWindow(messages);

    REQUIRE(window.size() == 3);
    CHECK(window.at(0).content == "system");
    CHECK(window.at(1).content == "new user");
    CHECK(window.at(2).content == "new assistant");
}

TEST_CASE("ContextManager returns all messages when under limit") {
    const std::vector<Message> messages = {
        message(Role::User, "one"),
        message(Role::Assistant, "two"),
    };

    // "one"(1) + "two"(1) ≈ 2 tokens, budget of 5 is plenty
    const auto window = ContextManager(5).buildWindow(messages);

    CHECK(window.size() == messages.size());
    CHECK(window.at(0).content == "one");
    CHECK(window.at(1).content == "two");
}

// Edge case 1: a tool-call group that doesn't fit the remaining budget
// must be dropped entirely — never split.
TEST_CASE("ContextManager drops entire tool-call group when it exceeds budget") {
    const std::vector<Message> messages = {
        message(Role::System, "sys"),
        message(Role::User, "read a file"),
        assistantWithToolCalls("ok", {{"read_file", "{\"path\":\"x\"}"}}),
        toolResult("call_read_file", std::string(100, 'x')),  // 100 chars ≈ 25 tokens
        message(Role::User, "next question"),
        message(Role::Assistant, "all done"),
    };

    // Budget = 8 tokens: System(1) + "next question"(2) + "all done"(2)
    //                  = 5 tokens, leaving 3 — not enough for the tool group (~26)
    // The tool group must be dropped entirely.
    const auto window = ContextManager(8).buildWindow(messages);

    // Should contain: System + "next question" + "all done" (no orphaned tool_calls)
    REQUIRE(window.size() == 3);
    CHECK(window.at(0).content == "sys");
    CHECK(window.at(1).content == "next question");
    CHECK(window.at(2).content == "all done");

    // Verify no Assistant message with tool_calls survived
    for (const auto& m : window) {
        CHECK(m.toolCalls.empty());
    }
}

// Edge case 2: large System Prompt takes most of the budget, but small
// recent messages still fit.
TEST_CASE("ContextManager handles large system prompt gracefully") {
    // System Prompt = 800 chars ≈ 200 tokens
    const std::vector<Message> messages = {
        message(Role::System, std::string(800, '#')),
        message(Role::User, "hi"),
        message(Role::Assistant, "hello"),
    };

    // Budget = 210 tokens: System(200) + User(1) + Assistant(1) = 202, fits
    const auto window = ContextManager(210).buildWindow(messages);
    CHECK(window.size() == 3);

    // Budget = 201 tokens: System(200) + Assistant(1) = 201, only 2 fit
    const auto tight = ContextManager(201).buildWindow(messages);
    REQUIRE(tight.size() == 2);
    CHECK(tight.at(0).content.size() == 800);  // System always preserved
    CHECK(tight.at(1).content == "hello");      // newest non-system
}

// Edge case 3: two independent tool-call groups separated by a User message
// are treated as separate atomic units.
TEST_CASE("ContextManager treats adjacent tool-call groups independently") {
    const std::vector<Message> messages = {
        message(Role::System, "sys"),
        // Group 1: read_file + result
        message(Role::User, "read a.txt"),
        assistantWithToolCalls("reading", {{"read_file", "{}"}}),
        toolResult("call_read_file", "content of a"),
        // Separator
        message(Role::User, "now read b.txt"),
        // Group 2: read_file + two results
        assistantWithToolCalls("reading both", {
            {"read_file", "{}"},
            {"list_dir", "{}"},
        }),
        toolResult("call_read_file", "content of b"),
        toolResult("call_list_dir", "file list here"),
        message(Role::Assistant, "summary of both files"),
    };

    // Budget = 35: System(1) + G4(27) + G5(5) = 33, fits.
    // G1–G2 (first tool group, 16 tokens) dropped entirely.
    const auto window = ContextManager(35).buildWindow(messages);

    // Group 1 should be dropped entirely
    bool foundReadA = false;
    for (const auto& m : window) {
        if (m.content.find("content of a") != std::string::npos) {
            foundReadA = true;
        }
    }
    CHECK_FALSE(foundReadA);  // Group 1 dropped

    // Group 2 must be complete: Assistant + 2 Tools + summary
    CHECK(window.size() >= 5);  // System + Group2(4) + summary(1) = 6
    CHECK(window.at(0).content == "sys");

    // Find the Group 2 assistant
    bool foundGroup2 = false;
    for (const auto& m : window) {
        if (m.content == "reading both") {
            foundGroup2 = true;
            CHECK(m.toolCalls.size() == 2);
            break;
        }
    }
    CHECK(foundGroup2);
}
