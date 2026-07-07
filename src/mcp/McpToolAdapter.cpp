#include "mcp/McpToolAdapter.h"

#include <exception>
#include <utility>

namespace cpp_ai_agent::mcp {

McpToolAdapter::McpToolAdapter(
    std::vector<std::string> command,
    std::string exposedName,
    McpTool tool
)
    : command_(std::move(command)), exposedName_(std::move(exposedName)), tool_(std::move(tool)) {}

std::string McpToolAdapter::name() const {
    return exposedName_;
}

std::string McpToolAdapter::description() const {
    return "[MCP] " + tool_.description;
}

nlohmann::json McpToolAdapter::parametersSchema() const {
    return tool_.inputSchema.empty() ? nlohmann::json{{"type", "object"}} : tool_.inputSchema;
}

tools::RiskLevel McpToolAdapter::risk() const {
    return tools::RiskLevel::Safe;
}

tools::ToolResult McpToolAdapter::execute(const nlohmann::json& args) const {
    try {
        StdioMcpClient client(command_);
        const auto result = client.callTool(tool_.name, args);
        if (result.isError) {
            return {false, result.text, result.text};
        }
        return {true, result.text, result.text};
    } catch (const std::exception& ex) {
        return {false, std::string("MCP tool failed: ") + ex.what(), ""};
    }
}

}  // namespace cpp_ai_agent::mcp
