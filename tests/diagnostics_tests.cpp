#include <doctest/doctest.h>

#include "diagnostics/Diagnostics.h"

#include <algorithm>
#include <string>

TEST_CASE("Config summary reports key presence without leaking the key") {
    cpp_ai_agent::config::AppConfig config;
    config.llm.baseUrl = "https://api.example/v1";
    config.llm.model = "test-model";
    config.llm.apiKey = "sk-secret-value";
    config.webSearchProxyUrl = "http://127.0.0.1:7897";
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
    CHECK(std::find(lines.begin(), lines.end(), "web_search_proxy_url: http://127.0.0.1:7897") != lines.end());
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

TEST_CASE("Diagnostics warns when base URL includes chat completions path") {
    cpp_ai_agent::config::AppConfig config;
    config.llm.baseUrl = "https://api.linkapi.ai/v1/chat/completions";
    config.llm.model = "gpt-5.4-mini";
    config.llm.apiKey = "test-key";
    config.workspaceRoot = ".";
    config.historyDir = "logs";

    const auto lines =
        cpp_ai_agent::diagnostics::formatDiagnostics(cpp_ai_agent::diagnostics::runLocalDiagnostics(config));

    bool sawWarning = false;
    for (const auto& line : lines) {
        if (line.find("do not include /chat/completions") != std::string::npos) {
            sawWarning = true;
        }
    }

    CHECK(sawWarning);
}

TEST_CASE("Diagnostics accepts DeepSeek OpenAI-compatible configuration") {
    cpp_ai_agent::config::AppConfig config;
    config.llm.baseUrl = "https://api.deepseek.com";
    config.llm.model = "deepseek-chat";
    config.llm.apiKey = "test-key";
    config.workspaceRoot = ".";
    config.historyDir = "logs";

    const auto lines =
        cpp_ai_agent::diagnostics::formatDiagnostics(cpp_ai_agent::diagnostics::runLocalDiagnostics(config));

    bool sawBaseUrlOk = false;
    bool sawModelOk = false;
    for (const auto& line : lines) {
        if (line == "[ok] OPENAI_BASE_URL: https://api.deepseek.com") {
            sawBaseUrlOk = true;
        }
        if (line == "[ok] OPENAI_MODEL: deepseek-chat") {
            sawModelOk = true;
        }
    }

    CHECK(sawBaseUrlOk);
    CHECK(sawModelOk);
}

TEST_CASE("Diagnostics warns for unusual DeepSeek model") {
    cpp_ai_agent::config::AppConfig config;
    config.llm.baseUrl = "https://api.deepseek.com";
    config.llm.model = "gpt-5.4-mini";
    config.llm.apiKey = "test-key";
    config.workspaceRoot = ".";
    config.historyDir = "logs";

    const auto lines =
        cpp_ai_agent::diagnostics::formatDiagnostics(cpp_ai_agent::diagnostics::runLocalDiagnostics(config));

    bool sawWarning = false;
    for (const auto& line : lines) {
        if (line.find("DeepSeek model should usually be") != std::string::npos) {
            sawWarning = true;
        }
    }

    CHECK(sawWarning);
}
