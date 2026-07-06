#pragma once

#include "llm/LlmClient.h"
#include "security/PermissionManager.h"

#include <string>

namespace cpp_ai_agent::config {

struct AppConfig {
    llm::LlmConfig llm;
    int maxIterations = 10;
    security::PermissionMode permissionMode = security::PermissionMode::AskEachTime;
    std::string workspaceRoot = ".";
    std::string historyDir = "logs";
};

AppConfig loadAppConfig(const std::string& path);

}  // namespace cpp_ai_agent::config
