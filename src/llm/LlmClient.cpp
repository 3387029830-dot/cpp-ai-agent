#include "llm/LlmClient.h"

#include "core/Message.h"
#include "llm/LlmParsing.h"

#include <cpr/cpr.h>
#include <cpr/sse.h>
#include <cpr/ssl_options.h>
#include <nlohmann/json.hpp>

#include <map>
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
        nlohmann::json apiMessage = {
            {"role", core::roleToApiString(message.role)},
            {"content", message.content},
        };

        if (!message.toolCalls.empty()) {
            nlohmann::json toolCalls = nlohmann::json::array();
            for (const auto& toolCall : message.toolCalls) {
                toolCalls.push_back({
                    {"id", toolCall.id},
                    {"type", "function"},
                    {"function",
                     {
                         {"name", toolCall.name},
                         {"arguments", toolCall.arguments},
                     }},
                });
            }
            apiMessage["tool_calls"] = toolCalls;
        }

        if (message.role == core::Role::Tool) {
            apiMessage["tool_call_id"] = message.toolCallId;
        }

        apiMessages.push_back(std::move(apiMessage));
    }

    return apiMessages;
}

std::string redactSensitiveText(const std::string& text) {
    return std::regex_replace(text, std::regex(R"(sk-[^"'\s,}]+)"), "sk-***REDACTED***");
}

/// Holds incremental tool-call data assembled from SSE deltas.
/// OpenAI streams tool_calls piecemeal: each delta carries an index,
/// and arguments arrive across multiple events.
struct ToolCallFragment {
    std::string id;
    std::string name;
    std::string arguments;
};

std::vector<core::ToolCall> buildToolCalls(const std::map<int, ToolCallFragment>& fragments) {
    std::vector<core::ToolCall> result;
    for (const auto& [index, frag] : fragments) {
        core::ToolCall tc;
        tc.id = frag.id;
        tc.name = frag.name;
        tc.arguments = frag.arguments;
        result.push_back(std::move(tc));
    }
    return result;
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

// ── ILlmClient convenience methods ──────────────────────────────────

core::Message ILlmClient::chat(const std::vector<core::Message>& messages) const {
    return chatStream(messages, nlohmann::json::array(), {});
}

core::Message ILlmClient::chat(
    const std::vector<core::Message>& messages,
    const nlohmann::json& toolsSpec
) const {
    return chatStream(messages, toolsSpec, {});
}

void LlmClient::configureSession(cpr::Session& session,
                                  const std::string& url,
                                  const std::string& body) const {
    session.SetUrl(cpr::Url{url});
    session.SetHeader(cpr::Header{
        {"Authorization", "Bearer " + config_.apiKey},
        {"Content-Type", "application/json"},
    });
    session.SetBody(cpr::Body{body});

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
}

core::Message LlmClient::chatStream(
    const std::vector<core::Message>& messages,
    const nlohmann::json& toolsSpec,
    ChunkCallback onChunk
) const {
    const auto url = trimTrailingSlash(config_.baseUrl) + "/chat/completions";
    nlohmann::json requestBody = {
        {"model", config_.model},
        {"messages", toApiMessages(messages)},
    };

    if (!toolsSpec.empty()) {
        requestBody["tools"] = toolsSpec;
        requestBody["tool_choice"] = "auto";
    }

    // ── Non-streaming path: no callback → traditional HTTP ─────────
    // Preserves response.text for error reporting.
    if (!onChunk) {
        cpr::Session session;
        configureSession(session, url, requestBody.dump());

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
        return parseAssistantMessage(body.at("choices").at(0).at("message"));
    }

    // ── Streaming path: SSE callback → real-time token delivery ────
    requestBody["stream"] = true;

    std::string accumulatedContent;
    std::map<int, ToolCallFragment> toolCallFragments;

    cpr::Session session;
    configureSession(session, url, requestBody.dump());

    session.SetServerSentEventCallback(cpr::ServerSentEventCallback{
        [&](cpr::ServerSentEvent&& event, intptr_t /*userdata*/) -> bool {
            if (event.data == "[DONE]") {
                return true;
            }

            try {
                const auto json = nlohmann::json::parse(event.data);
                const auto& choices = json.at("choices");
                if (choices.empty()) {
                    return true;
                }

                const auto& delta = choices[0].value("delta", nlohmann::json::object());

                // Text content chunk
                if (delta.contains("content")) {
                    const auto& content = delta["content"];
                    if (content.is_string() && !content.get_ref<const std::string&>().empty()) {
                        const auto& chunk = content.get_ref<const std::string&>();
                        accumulatedContent += chunk;
                        onChunk(chunk);
                    }
                }

                // Tool-call delta(s) — accumulate by index
                if (delta.contains("tool_calls")) {
                    for (const auto& tc : delta["tool_calls"]) {
                        const int index = tc.value("index", 0);
                        auto& frag = toolCallFragments[index];

                        if (tc.contains("id")) {
                            frag.id = tc["id"].get<std::string>();
                        }
                        if (tc.contains("function")) {
                            const auto& func = tc["function"];
                            if (func.contains("name")) {
                                frag.name = func["name"].get<std::string>();
                            }
                            if (func.contains("arguments")) {
                                frag.arguments += func["arguments"].get<std::string>();
                            }
                        }
                    }
                }
            } catch (const std::exception&) {
                // Malformed SSE event — skip it.
            }

            return true;
        }
    });

    const auto response = session.Post();

    if (response.error) {
        throw std::runtime_error("HTTP request failed: " + response.error.message);
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        throw std::runtime_error(
            "LLM API returned HTTP " + std::to_string(response.status_code)
        );
    }

    core::Message assistant;
    assistant.role = core::Role::Assistant;
    assistant.content = accumulatedContent;
    assistant.toolCalls = buildToolCalls(toolCallFragments);
    return assistant;
}

}  // namespace cpp_ai_agent::llm
