#include <doctest/doctest.h>

#include "config/AppConfig.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

namespace {

class ScopedEnv {
public:
    explicit ScopedEnv(std::string name) : name_(std::move(name)) {
#ifdef _WIN32
        char* rawValue = nullptr;
        std::size_t valueSize = 0;
        if (_dupenv_s(&rawValue, &valueSize, name_.c_str()) == 0 && rawValue != nullptr) {
            std::unique_ptr<char, decltype(&std::free)> value(rawValue, &std::free);
            oldValue_ = value.get();
            hadOldValue_ = true;
        }
#else
        const char* value = std::getenv(name_.c_str());
        if (value != nullptr) {
            oldValue_ = value;
            hadOldValue_ = true;
        }
#endif
    }

    ~ScopedEnv() {
        if (hadOldValue_) {
            set(oldValue_);
        } else {
            unset();
        }
    }

    void set(const std::string& value) const {
#ifdef _WIN32
        _putenv_s(name_.c_str(), value.c_str());
#else
        setenv(name_.c_str(), value.c_str(), 1);
#endif
    }

    void unset() const {
#ifdef _WIN32
        _putenv_s(name_.c_str(), "");
#else
        unsetenv(name_.c_str());
#endif
    }

private:
    std::string name_;
    std::string oldValue_;
    bool hadOldValue_ = false;
};

std::filesystem::path makeTempConfigWorkspace(const std::string& name) {
    auto root = std::filesystem::temp_directory_path() / ("cpp-ai-agent-config-" + name);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "config");
    return root;
}

void writeTextFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
}

std::string settingsJson() {
    return R"({
  "llm": {
    "base_url": "https://config.example/v1",
    "model": "config-model",
    "api_key_env": "OPENAI_API_KEY",
    "proxy_url": ""
  },
  "web_search": {
    "proxy_url": "http://127.0.0.1:7897"
  },
  "agent": {
    "max_iterations": 10
  }
})";
}

}  // namespace

TEST_CASE("AppConfig loads LLM values from .env when process environment is empty") {
    ScopedEnv apiKey("OPENAI_API_KEY");
    ScopedEnv baseUrl("OPENAI_BASE_URL");
    ScopedEnv model("OPENAI_MODEL");
    ScopedEnv proxy("OPENAI_PROXY_URL");
    apiKey.unset();
    baseUrl.unset();
    model.unset();
    proxy.unset();

    const auto root = makeTempConfigWorkspace("dotenv");
    writeTextFile(root / "config" / "settings.json", settingsJson());
    writeTextFile(
        root / ".env",
        "OPENAI_API_KEY=dotenv-key\n"
        "OPENAI_BASE_URL=https://dotenv.example/v1\n"
        "OPENAI_MODEL=dotenv-model\n"
        "OPENAI_PROXY_URL=\n"
    );

    const auto config =
        cpp_ai_agent::config::loadAppConfig((root / "config" / "settings.json").string());

    CHECK(config.llm.apiKey == "dotenv-key");
    CHECK(config.llm.baseUrl == "https://dotenv.example/v1");
    CHECK(config.llm.model == "dotenv-model");
    CHECK(config.llm.proxyUrl.empty());
    CHECK(config.webSearchProxyUrl == "http://127.0.0.1:7897");

    std::filesystem::remove_all(root);
}

TEST_CASE("AppConfig loads dedicated web search proxy with environment priority") {
    ScopedEnv webSearchProxy("WEB_SEARCH_PROXY_URL");
    webSearchProxy.set("http://127.0.0.1:10809");

    const auto root = makeTempConfigWorkspace("web-search-proxy");
    writeTextFile(root / "config" / "settings.json", settingsJson());

    const auto config =
        cpp_ai_agent::config::loadAppConfig((root / "config" / "settings.json").string());

    CHECK(config.webSearchProxyUrl == "http://127.0.0.1:10809");
    CHECK(config.llm.proxyUrl.empty());

    std::filesystem::remove_all(root);
}

TEST_CASE("AppConfig gives process environment priority over .env") {
    ScopedEnv apiKey("OPENAI_API_KEY");
    ScopedEnv model("OPENAI_MODEL");
    apiKey.set("env-key");
    model.set("env-model");

    const auto root = makeTempConfigWorkspace("env-priority");
    writeTextFile(root / "config" / "settings.json", settingsJson());
    writeTextFile(
        root / ".env",
        "OPENAI_API_KEY=dotenv-key\n"
        "OPENAI_MODEL=dotenv-model\n"
    );

    const auto config =
        cpp_ai_agent::config::loadAppConfig((root / "config" / "settings.json").string());

    CHECK(config.llm.apiKey == "env-key");
    CHECK(config.llm.model == "env-model");

    std::filesystem::remove_all(root);
}

TEST_CASE("AppConfig normalizes common LinkAPI model typo") {
    ScopedEnv model("OPENAI_MODEL");
    model.set("gpt-5.4.-mini");

    const auto root = makeTempConfigWorkspace("model-typo");
    writeTextFile(root / "config" / "settings.json", settingsJson());

    const auto config =
        cpp_ai_agent::config::loadAppConfig((root / "config" / "settings.json").string());

    CHECK(config.llm.model == "gpt-5.4-mini");

    std::filesystem::remove_all(root);
}
