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

}  // namespace

std::vector<DiagnosticItem> runLocalDiagnostics(const config::AppConfig& config) {
    std::vector<DiagnosticItem> items;

    if (config.llm.baseUrl.empty()) {
        items.push_back(warn("OPENAI_BASE_URL", "missing base_url"));
    } else {
        items.push_back(ok("OPENAI_BASE_URL", config.llm.baseUrl));
    }

    if (config.llm.model.empty()) {
        items.push_back(warn("OPENAI_MODEL", "missing model"));
    } else if (config.llm.model == "gpt-5.3-codex") {
        items.push_back(warn("OPENAI_MODEL", "gpt-5.3-codex is not available for the current LinkAPI account; use gpt-5.4-mini"));
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
