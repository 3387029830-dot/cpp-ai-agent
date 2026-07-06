#pragma once

#include "llm/LlmClient.h"

#include <string>

namespace cpp_ai_agent::config {

struct AppConfig {
    llm::LlmConfig llm;
    int maxIterations = 10;
    std::string workspaceRoot = ".";
};

AppConfig loadAppConfig(const std::string& path);

}  // namespace cpp_ai_agent::config
