#pragma once

#include "tools/ITool.h"

namespace cpp_ai_agent::tools {

class ShellTool final : public ITool {
public:
    std::string name() const override;
    std::string description() const override;
    nlohmann::json parametersSchema() const override;
    RiskLevel risk() const override;
    ToolResult execute(const nlohmann::json& args) const override;
};

}  // namespace cpp_ai_agent::tools
