#include <doctest/doctest.h>

#include "mcp/McpClient.h"

#include <nlohmann/json.hpp>

using cpp_ai_agent::mcp::makeInitializeRequest;
using cpp_ai_agent::mcp::makeInitializedNotification;
using cpp_ai_agent::mcp::makeToolsListRequest;
using cpp_ai_agent::mcp::parseInitializeResult;
using cpp_ai_agent::mcp::parseToolsListResult;

TEST_CASE("McpClient builds initialize and tools/list requests") {
    const auto initialize = makeInitializeRequest(1);

    CHECK(initialize.at("jsonrpc") == "2.0");
    CHECK(initialize.at("id") == 1);
    CHECK(initialize.at("method") == "initialize");
    CHECK(initialize.at("params").at("clientInfo").at("name") == "cpp-ai-agent");

    const auto initialized = makeInitializedNotification();
    CHECK(initialized.at("method") == "notifications/initialized");
    CHECK_FALSE(initialized.contains("id"));

    const auto toolsList = makeToolsListRequest(2);
    CHECK(toolsList.at("id") == 2);
    CHECK(toolsList.at("method") == "tools/list");
}

TEST_CASE("McpClient parses initialize and tools/list responses") {
    const nlohmann::json initializeResponse = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"result",
         {
             {"protocolVersion", "2025-03-26"},
             {"serverInfo", {{"name", "demo-server"}, {"version", "1.0.0"}}},
             {"instructions", "demo instructions"},
         }},
    };

    const auto server = parseInitializeResult(initializeResponse);
    CHECK(server.protocolVersion == "2025-03-26");
    CHECK(server.name == "demo-server");
    CHECK(server.version == "1.0.0");
    CHECK(server.instructions == "demo instructions");

    const nlohmann::json toolsResponse = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"result",
         {
             {"tools",
              nlohmann::json::array({
                  {
                      {"name", "echo"},
                      {"description", "Echo text."},
                      {"inputSchema", {{"type", "object"}}},
                  },
                  {
                      {"description", "Missing name should be ignored."},
                      {"inputSchema", {{"type", "object"}}},
                  },
              })},
         }},
    };

    const auto tools = parseToolsListResult(toolsResponse);
    REQUIRE(tools.size() == 1);
    CHECK(tools.at(0).name == "echo");
    CHECK(tools.at(0).description == "Echo text.");
    CHECK(tools.at(0).inputSchema.at("type") == "object");
}
