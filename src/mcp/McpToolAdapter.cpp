#include "mcp/McpToolAdapter.h"

#include <cctype>
#include <exception>
#include <memory>
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
        // Lazily create the persistent client on first call; subsequent calls
        // reuse the same process connection (initialize handshake is done once).
        if (!client_) {
            client_ = std::make_unique<StdioMcpClient>(command_);
        }
        const auto result = client_->callTool(tool_.name, args);
        if (result.isError) {
            return {false, result.text, result.text};
        }
        return {true, result.text, result.text};
    } catch (const std::exception& ex) {
        // If the process died or the connection was lost, reset the client so
        // the next call will automatically spawn a fresh process and reconnect.
        client_.reset();
        return {false, std::string("MCP tool failed: ") + ex.what(), ""};
    }
}

}  // namespace cpp_ai_agent::mcp
