#include "diagnostics/Diagnostics.h"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace cpp_ai_agent::diagnostics {

namespace {

std::string getEnvValue(const std::string& name) {
#ifdef _WIN32
    char* rawValue = nullptr;
    std::size_t valueSize = 0;
    if (_dupenv_s(&rawValue, &valueSize, name.c_str()) != 0 || rawValue == nullptr) {
        return "";
    }

    std::string value(rawValue);
    std::free(rawValue);
    return value;
#else
    const char* value = std::getenv(name.c_str());
    return value == nullptr ? "" : value;
#endif
}

DiagnosticItem ok(std::string name, std::string detail) {
    return {"ok", std::move(name), std::move(detail)};
}

DiagnosticItem warn(std::string name, std::string detail) {
    return {"warn", std::move(name), std::move(detail)};
}

bool endsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool contains(const std::string& value, const std::string& fragment) {
    return value.find(fragment) != std::string::npos;
}

bool isLinkApiBaseUrl(const std::string& baseUrl) {
    return contains(baseUrl, "api.linkapi.ai");
}

bool isDeepSeekBaseUrl(const std::string& baseUrl) {
    return contains(baseUrl, "api.deepseek.com");
}

}  // namespace

std::vector<DiagnosticItem> runLocalDiagnostics(const config::AppConfig& config) {
    std::vector<DiagnosticItem> items;

    if (config.llm.baseUrl.empty()) {
        items.push_back(warn("OPENAI_BASE_URL", "missing base_url"));
    } else if (endsWith(config.llm.baseUrl, "/chat/completions")) {
        items.push_back(warn("OPENAI_BASE_URL", "base_url must stop at /v1; do not include /chat/completions"));
    } else if (isLinkApiBaseUrl(config.llm.baseUrl) && config.llm.baseUrl.find("/v1") == std::string::npos) {
        items.push_back(warn("OPENAI_BASE_URL", "LinkAPI base_url should be https://api.linkapi.ai/v1"));
    } else {
        items.push_back(ok("OPENAI_BASE_URL", config.llm.baseUrl));
    }

    if (config.llm.model.empty()) {
        items.push_back(warn("OPENAI_MODEL", "missing model"));
    } else if (isLinkApiBaseUrl(config.llm.baseUrl) && config.llm.model == "gpt-5.3-codex") {
        items.push_back(warn("OPENAI_MODEL", "gpt-5.3-codex is not available for the current LinkAPI account; use gpt-5.4-mini"));
    } else if (isLinkApiBaseUrl(config.llm.baseUrl) && config.llm.model != "gpt-5.4-mini" && config.llm.model != "gpt-5.4" && config.llm.model != "gpt-5.5") {
        items.push_back(warn("OPENAI_MODEL", "model is not one of the LinkAPI models verified for this project: gpt-5.4-mini, gpt-5.4, gpt-5.5"));
    } else if (isDeepSeekBaseUrl(config.llm.baseUrl) && config.llm.model != "deepseek-chat" && config.llm.model != "deepseek-reasoner") {
        items.push_back(warn("OPENAI_MODEL", "DeepSeek model should usually be deepseek-chat or deepseek-reasoner"));
    } else {
        items.push_back(ok("OPENAI_MODEL", config.llm.model));
    }

    if (config.llm.apiKey.empty() || config.llm.apiKey == "your_api_key_here") {
        items.push_back(warn("OPENAI_API_KEY", "missing or still using placeholder"));
    } else {
        items.push_back(ok("OPENAI_API_KEY", "set"));
    }

    const auto vcpkgRoot = getEnvValue("VCPKG_ROOT");
    if (vcpkgRoot.empty()) {
        items.push_back(warn("VCPKG_ROOT", "not set; cmake preset may fail on a fresh machine"));
    } else {
        const auto toolchain = std::filesystem::path(vcpkgRoot) / "scripts" / "buildsystems" / "vcpkg.cmake";
        if (std::filesystem::exists(toolchain)) {
            items.push_back(ok("VCPKG_ROOT", vcpkgRoot));
        } else {
            items.push_back(warn("VCPKG_ROOT", "set but vcpkg.cmake was not found: " + toolchain.string()));
        }
    }

    if (std::filesystem::exists(config.workspaceRoot)) {
        items.push_back(ok("workspace_root", config.workspaceRoot));
    } else {
        items.push_back(warn("workspace_root", "path does not exist: " + config.workspaceRoot));
    }

    items.push_back(ok("history_dir", config.historyDir));
    return items;
}

std::vector<std::string> formatDiagnostics(const std::vector<DiagnosticItem>& items) {
    std::vector<std::string> lines;
    for (const auto& item : items) {
        lines.push_back("[" + item.status + "] " + item.name + ": " + item.detail);
    }
    return lines;
}

std::vector<std::string> formatConfigSummary(const config::AppConfig& config) {
    return {
        "base_url: " + config.llm.baseUrl,
        "model: " + config.llm.model,
        "api_key: " + std::string(config.llm.apiKey.empty() ? "missing" : "set"),
        "proxy_url: " + std::string(config.llm.proxyUrl.empty() ? "(empty)" : config.llm.proxyUrl),
        "workspace_root: " + config.workspaceRoot,
        "history_dir: " + config.historyDir,
    };
}

}  // namespace cpp_ai_agent::diagnostics
