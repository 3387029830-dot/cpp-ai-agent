#include "mcp/McpToolAdapter.h"

#include <cctype>
#include <exception>
#include <utility>

namespace cpp_ai_agent::mcp {

std::string makeMcpToolName(const std::string& serverName, const std::string& toolName) {
    auto appendSanitized = [](std::string& output, const std::string& value) {
        for (const unsigned char ch : value) {
            if (std::isalnum(ch) != 0 || ch == '_') {
                output.push_back(static_cast<char>(std::tolower(ch)));
            } else {
                output.push_back('_');
            }
        }
    };

    std::string result = "mcp_";
    appendSanitized(result, serverName);
    result.push_back('_');
    appendSanitized(result, toolName);

    while (result.find("__") != std::string::npos) {
        result.replace(result.find("__"), 2, "_");
    }
    if (!result.empty() && result.back() == '_') {
        result.pop_back();
    }
    return result;
}

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
