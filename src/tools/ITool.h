#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace cpp_ai_agent::tools {

enum class RiskLevel {
    Safe,
    Write,
    Dangerous,
};

struct ToolResult {
    bool success = false;
    std::string output;
    std::string display;
};

class ITool {
public:
    virtual ~ITool() = default;

    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual nlohmann::json parametersSchema() const = 0;
    virtual RiskLevel risk() const = 0;
    virtual ToolResult execute(const nlohmann::json& args) const = 0;
};

}  // namespace cpp_ai_agent::tools
