#include <doctest/doctest.h>

#include "mcp/McpClient.h"
#include "mcp/McpToolAdapter.h"

#include <nlohmann/json.hpp>

using cpp_ai_agent::mcp::makeInitializeRequest;
using cpp_ai_agent::mcp::makeInitializedNotification;
using cpp_ai_agent::mcp::makeToolsCallRequest;
using cpp_ai_agent::mcp::makeToolsListRequest;
using cpp_ai_agent::mcp::parseInitializeResult;
using cpp_ai_agent::mcp::parseToolsCallResult;
using cpp_ai_agent::mcp::parseToolsListResult;

TEST_CASE("McpToolAdapter exposes MCP metadata as a local tool") {
    cpp_ai_agent::mcp::McpTool mcpTool;
    mcpTool.name = "echo";
    mcpTool.description = "Echo text.";
    mcpTool.inputSchema = {
        {"type", "object"},
        {"properties", {{"text", {{"type", "string"}}}}},
    };

    cpp_ai_agent::mcp::McpToolAdapter adapter({"ai-agent.exe", "/mcp-test-server"}, "mcp_echo", mcpTool);

    CHECK(adapter.name() == "mcp_echo");
    CHECK(adapter.description().find("[MCP]") == 0);
    CHECK(adapter.parametersSchema().at("properties").contains("text"));
    CHECK(adapter.risk() == cpp_ai_agent::tools::RiskLevel::Safe);
}

TEST_CASE("McpClient builds initialize, tools/list, and tools/call requests") {
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

    const auto toolsCall = makeToolsCallRequest(3, "echo", {{"text", "hello"}});
    CHECK(toolsCall.at("id") == 3);
    CHECK(toolsCall.at("method") == "tools/call");
    CHECK(toolsCall.at("params").at("name") == "echo");
    CHECK(toolsCall.at("params").at("arguments").at("text") == "hello");
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

TEST_CASE("McpClient parses tools/call text results") {
    const nlohmann::json response = {
        {"jsonrpc", "2.0"},
        {"id", 3},
        {"result",
         {
             {"content",
              nlohmann::json::array({
                  {{"type", "text"}, {"text", "hello"}},
                  {{"type", "text"}, {"text", "world"}},
              })},
             {"isError", false},
         }},
    };

    const auto result = parseToolsCallResult(response);

    CHECK_FALSE(result.isError);
    CHECK(result.text == "hello\nworld");
    REQUIRE(result.rawContent.size() == 2);
}
