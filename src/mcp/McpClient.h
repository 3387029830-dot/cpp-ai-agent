#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace cpp_ai_agent::mcp {

struct McpTool {
    std::string name;
    std::string description;
    nlohmann::json inputSchema;
};

struct McpServerInfo {
    std::string protocolVersion;
    std::string name;
    std::string version;
    std::string instructions;
    std::vector<McpTool> tools;
};

nlohmann::json makeInitializeRequest(int id);
nlohmann::json makeInitializedNotification();
nlohmann::json makeToolsListRequest(int id);
McpServerInfo parseInitializeResult(const nlohmann::json& response);
std::vector<McpTool> parseToolsListResult(const nlohmann::json& response);

class StdioMcpClient {
public:
    explicit StdioMcpClient(std::vector<std::string> command);

    McpServerInfo discoverTools() const;

private:
    std::vector<std::string> command_;
};

}  // namespace cpp_ai_agent::mcp
