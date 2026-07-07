#pragma once

#include "mcp/McpClient.h"
#include "tools/ITool.h"

#include <string>
#include <vector>

namespace cpp_ai_agent::mcp {

class McpToolAdapter final : public tools::ITool {
public:
    McpToolAdapter(
        std::vector<std::string> command,
        std::string exposedName,
        McpTool tool
    );

    std::string name() const override;
    std::string description() const override;
    nlohmann::json parametersSchema() const override;
    tools::RiskLevel risk() const override;
    tools::ToolResult execute(const nlohmann::json& args) const override;

private:
    std::vector<std::string> command_;
    std::string exposedName_;
    McpTool tool_;
};

}  // namespace cpp_ai_agent::mcp
