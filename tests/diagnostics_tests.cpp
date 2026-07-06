#include <doctest/doctest.h>

#include "diagnostics/Diagnostics.h"

#include <string>

TEST_CASE("Config summary reports key presence without leaking the key") {
    cpp_ai_agent::config::AppConfig config;
    config.llm.baseUrl = "https://api.example/v1";
    config.llm.model = "test-model";
    config.llm.apiKey = "sk-secret-value";
    config.workspaceRoot = ".";
    config.historyDir = "logs";

    const auto lines = cpp_ai_agent::diagnostics::formatConfigSummary(config);

    bool sawKeyStatus = false;
    for (const auto& line : lines) {
        CHECK(line.find("sk-secret-value") == std::string::npos);
        if (line == "api_key: set") {
            sawKeyStatus = true;
        }
    }

    CHECK(sawKeyStatus);
}

TEST_CASE("Diagnostics warns for placeholder API key") {
    cpp_ai_agent::config::AppConfig config;
    config.llm.baseUrl = "https://api.example/v1";
    config.llm.model = "test-model";
    config.llm.apiKey = "your_api_key_here";
    config.workspaceRoot = ".";
    config.historyDir = "logs";

    const auto lines =
        cpp_ai_agent::diagnostics::formatDiagnostics(cpp_ai_agent::diagnostics::runLocalDiagnostics(config));

    bool sawWarning = false;
    for (const auto& line : lines) {
        if (line.find("[warn] OPENAI_API_KEY") != std::string::npos) {
            sawWarning = true;
        }
    }

    CHECK(sawWarning);
}
