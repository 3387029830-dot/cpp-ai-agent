#include "llm/LlmClient.h"

#include "core/Message.h"

#include <cpr/cpr.h>
#include <cpr/ssl_options.h>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <regex>
#include <utility>

namespace cpp_ai_agent::llm {

namespace {

std::string trimTrailingSlash(std::string value) {
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

nlohmann::json toApiMessages(const std::vector<core::Message>& messages) {
    nlohmann::json apiMessages = nlohmann::json::array();

    for (const auto& message : messages) {
        apiMessages.push_back({
            {"role", core::roleToApiString(message.role)},
            {"content", message.content},
        });
    }

    return apiMessages;
}

std::string redactSensitiveText(const std::string& text) {
    return std::regex_replace(text, std::regex(R"(sk-[^"'\s,}]+)"), "sk-***REDACTED***");
}

}  // namespace

LlmClient::LlmClient(LlmConfig config) : config_(std::move(config)) {
    if (config_.baseUrl.empty()) {
        throw std::runtime_error("LLM base_url is empty.");
    }
    if (config_.model.empty()) {
        throw std::runtime_error("LLM model is empty.");
    }
    if (config_.apiKey.empty()) {
        throw std::runtime_error("OPENAI_API_KEY is not set.");
    }
}

core::Message LlmClient::chat(const std::vector<core::Message>& messages) const {
    const auto url = trimTrailingSlash(config_.baseUrl) + "/chat/completions";
    const nlohmann::json requestBody = {
        {"model", config_.model},
        {"messages", toApiMessages(messages)},
    };

    cpr::Session session;
    session.SetUrl(cpr::Url{url});
    session.SetHeader(cpr::Header{
        {"Authorization", "Bearer " + config_.apiKey},
        {"Content-Type", "application/json"},
    });
    session.SetBody(cpr::Body{requestBody.dump()});

#ifdef _WIN32
    // Schannel can fail when Windows cannot reach certificate revocation servers.
    session.SetSslOptions(cpr::Ssl(cpr::ssl::NoRevoke{true}));
#endif

    if (!config_.proxyUrl.empty()) {
        session.SetProxies(cpr::Proxies{
            {"http", config_.proxyUrl},
            {"https", config_.proxyUrl},
        });
    }

    const auto response = session.Post();

    if (response.error) {
        throw std::runtime_error("HTTP request failed: " + response.error.message);
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        throw std::runtime_error(
            "LLM API returned HTTP " + std::to_string(response.status_code) + ": " +
            redactSensitiveText(response.text)
        );
    }

    const auto body = nlohmann::json::parse(response.text);
    const auto content = body.at("choices").at(0).at("message").value("content", "");

    core::Message assistant;
    assistant.role = core::Role::Assistant;
    assistant.content = content;
    return assistant;
}

}  // namespace cpp_ai_agent::llm
