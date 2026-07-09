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
