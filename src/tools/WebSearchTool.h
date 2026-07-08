#pragma once

#include "tools/ITool.h"

#include <string>

namespace cpp_ai_agent::tools {

class WebSearchTool final : public ITool {
public:
    explicit WebSearchTool(std::string proxyUrl = {});

    std::string name() const override;
    std::string description() const override;
    nlohmann::json parametersSchema() const override;
    RiskLevel risk() const override;
    ToolResult execute(const nlohmann::json& args) const override;

private:
    std::string proxyUrl_;
};

std::string formatDuckDuckGoResults(
    const std::string& query,
    const nlohmann::json& body,
    int maxResults
);

}  // namespace cpp_ai_agent::tools
