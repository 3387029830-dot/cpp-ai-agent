#pragma once

#include "tools/ITool.h"

#include <chrono>
#include <string>
#include <unordered_map>

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

    // Simple in-memory result cache to avoid redundant HTTP requests when the
    // model calls web_search for the same query within a short time window.
    static constexpr int cacheTtlSeconds_ = 300;
    static constexpr std::size_t cacheMaxEntries_ = 100;

    struct CachedEntry {
        ToolResult result;
        std::chrono::steady_clock::time_point timestamp;
    };
    mutable std::unordered_map<std::string, CachedEntry> cache_;
};

std::string formatDuckDuckGoResults(
    const std::string& query,
    const nlohmann::json& body,
    int maxResults
);

std::string formatBingRssResults(
    const std::string& query,
    const std::string& rss,
    int maxResults
);

}  // namespace cpp_ai_agent::tools
