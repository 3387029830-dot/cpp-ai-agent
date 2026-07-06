#include "config/AppConfig.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <memory>
#include <fstream>
#include <stdexcept>
#include <string>

namespace cpp_ai_agent::config {

namespace {

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

}  // namespace

AppConfig loadAppConfig(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    nlohmann::json json;
    input >> json;

    AppConfig config;
    const auto llm = json.at("llm");
    config.llm.baseUrl = llm.value("base_url", "https://api.openai.com/v1");
    config.llm.model = llm.value("model", "gpt-4.1-mini");
    config.llm.proxyUrl = llm.value("proxy_url", "");

    const auto apiKeyEnv = llm.value("api_key_env", "OPENAI_API_KEY");
    config.llm.apiKey = getEnvValue(apiKeyEnv);

    if (json.contains("agent")) {
        config.maxIterations = json.at("agent").value("max_iterations", 10);
    }

    return config;
}

}  // namespace cpp_ai_agent::config
