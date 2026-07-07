#include <doctest/doctest.h>

#include "llm/LlmParsing.h"

#include <nlohmann/json.hpp>

using cpp_ai_agent::core::Role;
using cpp_ai_agent::llm::parseAssistantMessage;

TEST_CASE("LlmParsing parses assistant tool calls") {
    const nlohmann::json apiMessage = {
        {"role", "assistant"},
        {"content", nullptr},
        {"tool_calls",
         nlohmann::json::array({
             {
                 {"id", "call-1"},
                 {"type", "function"},
                 {"function",
                  {
                      {"name", "read_file"},
                      {"arguments", R"({"path":"README.md"})"},
                  }},
             },
         })},
    };

    const auto message = parseAssistantMessage(apiMessage);

    CHECK(message.role == Role::Assistant);
    CHECK(message.content.empty());
    REQUIRE(message.toolCalls.size() == 1);
    CHECK(message.toolCalls.at(0).id == "call-1");
    CHECK(message.toolCalls.at(0).name == "read_file");
    CHECK(message.toolCalls.at(0).arguments == R"({"path":"README.md"})");
}

TEST_CASE("LlmParsing ignores malformed tool calls without names") {
    const nlohmann::json apiMessage = {
        {"role", "assistant"},
        {"content", "done"},
        {"tool_calls",
         nlohmann::json::array({
             {
                 {"id", "call-1"},
                 {"type", "function"},
                 {"function", {{"arguments", "{}"}}},
             },
         })},
    };

    const auto message = parseAssistantMessage(apiMessage);

    CHECK(message.content == "done");
    CHECK(message.toolCalls.empty());
}
