#pragma once

#include "tools/ITool.h"

#include <filesystem>

namespace cpp_ai_agent::tools {

class ReadFileTool final : public ITool {
public:
    explicit ReadFileTool(std::filesystem::path workspaceRoot);

    std::string name() const override;
    std::string description() const override;
    nlohmann::json parametersSchema() const override;
    RiskLevel risk() const override;
    ToolResult execute(const nlohmann::json& args) const override;

private:
    std::filesystem::path workspaceRoot_;
};

class WriteFileTool final : public ITool {
public:
    explicit WriteFileTool(std::filesystem::path workspaceRoot);

    std::string name() const override;
    std::string description() const override;
    nlohmann::json parametersSchema() const override;
    RiskLevel risk() const override;
    std::string preview(const nlohmann::json& args) const override;
    ToolResult execute(const nlohmann::json& args) const override;

private:
    std::filesystem::path workspaceRoot_;
};

class EditFileTool final : public ITool {
public:
    explicit EditFileTool(std::filesystem::path workspaceRoot);

    std::string name() const override;
    std::string description() const override;
    nlohmann::json parametersSchema() const override;
    RiskLevel risk() const override;
    std::string preview(const nlohmann::json& args) const override;
    ToolResult execute(const nlohmann::json& args) const override;

private:
    std::filesystem::path workspaceRoot_;
};

}  // namespace cpp_ai_agent::tools
