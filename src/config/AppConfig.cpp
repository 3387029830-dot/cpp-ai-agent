#include "config/AppConfig.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace cpp_ai_agent::config {

namespace {

using DotEnvValues = std::unordered_map<std::string, std::string>;

std::string getEnvValue(const std::string& name) {
#ifdef _MSC_VER
    char* rawValue = nullptr;
    std::size_t valueSize = 0;
    if (_dupenv_s(&rawValue, &valueSize, name.c_str()) != 0 || rawValue == nullptr) {
        return "";
    }

    std::unique_ptr<char, decltype(&std::free)> value(rawValue, &std::free);
    return std::string(value.get());
#else
    const char* value = std::getenv(name.c_str());
    if (value == nullptr) {
        return "";
    }
    return value;
#endif
}

std::string trim(std::string value) {
    const auto isSpace = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };

    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), isSpace));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), isSpace).base(), value.end());
    return value;
}

std::string unquote(std::string value) {
    if (value.size() < 2) {
        return value;
    }

    const auto first = value.front();
    const auto last = value.back();
    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
        return value.substr(1, value.size() - 2);
    }

    return value;
}

DotEnvValues loadDotEnvFile(const std::filesystem::path& path) {
    DotEnvValues values;

    std::ifstream input(path);
    if (!input) {
        return values;
    }

    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        auto key = trim(line.substr(0, separator));
        auto value = unquote(trim(line.substr(separator + 1)));
        if (!key.empty()) {
            values[key] = value;
        }
    }

    return values;
}

std::vector<std::filesystem::path> dotEnvCandidates(const std::string& configPath) {
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(".env");

    const auto config = std::filesystem::path(configPath);
    if (config.has_parent_path()) {
        candidates.push_back(config.parent_path() / ".env");
        candidates.push_back(config.parent_path().parent_path() / ".env");
    }

    return candidates;
}

DotEnvValues loadDotEnvValues(const std::string& configPath) {
    DotEnvValues values;
    for (const auto& path : dotEnvCandidates(configPath)) {
        auto fileValues = loadDotEnvFile(path);
        values.insert(fileValues.begin(), fileValues.end());
    }
    return values;
}

std::string getConfigValue(
    const DotEnvValues& dotEnv,
    const std::string& name,
    std::string fallback
) {
    auto value = getEnvValue(name);
    if (!value.empty()) {
        return value;
    }

    const auto it = dotEnv.find(name);
    if (it != dotEnv.end() && !it->second.empty()) {
        return it->second;
    }

    return fallback;
}

std::string normalizeModelName(std::string model) {
    if (model == "gpt-5.4.-mini") {
        return "gpt-5.4-mini";
    }
    return model;
}

}  // namespace

AppConfig loadAppConfig(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    nlohmann::json json;
    input >> json;

    const auto dotEnv = loadDotEnvValues(path);

    AppConfig config;
    const auto llm = json.at("llm");
    config.llm.baseUrl = getConfigValue(
        dotEnv,
        "OPENAI_BASE_URL",
        llm.value("base_url", "https://api.openai.com/v1")
    );
    config.llm.model = normalizeModelName(
        getConfigValue(dotEnv, "OPENAI_MODEL", llm.value("model", "gpt-4.1-mini"))
    );
    config.llm.proxyUrl = getConfigValue(dotEnv, "OPENAI_PROXY_URL", llm.value("proxy_url", ""));

    if (json.contains("web_search")) {
        config.webSearchProxyUrl = getConfigValue(
            dotEnv,
            "WEB_SEARCH_PROXY_URL",
            json.at("web_search").value("proxy_url", "")
        );
    } else {
        config.webSearchProxyUrl = getConfigValue(dotEnv, "WEB_SEARCH_PROXY_URL", "");
    }

    const auto apiKeyEnv = llm.value("api_key_env", "OPENAI_API_KEY");
    config.llm.apiKey = getConfigValue(dotEnv, apiKeyEnv, "");

    if (json.contains("agent")) {
        config.maxIterations = json.at("agent").value("max_iterations", 10);
    }

    if (json.contains("permission")) {
        config.permissionMode = security::permissionModeFromString(
            json.at("permission").value("mode", "ask_each_time")
        );
    }

    if (json.contains("paths")) {
        config.workspaceRoot = json.at("paths").value("workspace_root", ".");
        config.historyDir = json.at("paths").value("history_dir", "logs");
    }

    return config;
}

}  // namespace cpp_ai_agent::config
