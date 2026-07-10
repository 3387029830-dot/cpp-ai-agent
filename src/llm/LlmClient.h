#pragma once

#include "core/Message.h"

#include <nlohmann/json_fwd.hpp>

#include <functional>
#include <string>
#include <vector>

namespace cpr {
class Session;
}

namespace cpp_ai_agent::llm {

struct LlmConfig {
    std::string baseUrl;
    std::string apiKey;
    std::string model;
    std::string proxyUrl;
};

class ILlmClient {
public:
    using ChunkCallback = std::function<void(const std::string& chunk)>;

    virtual ~ILlmClient() = default;

    // Convenience overloads — non-virtual, delegate to chatStream.
    // Implemented in LlmClient.cpp (requires full nlohmann::json definition).
    core::Message chat(const std::vector<core::Message>& messages) const;
    core::Message chat(const std::vector<core::Message>& messages,
                       const nlohmann::json& toolsSpec) const;

    // The single pure-virtual entry point.
    virtual core::Message chatStream(
        const std::vector<core::Message>& messages,
        const nlohmann::json& toolsSpec,
        ChunkCallback onChunk
    ) const = 0;
};

class LlmClient : public ILlmClient {
public:
    explicit LlmClient(LlmConfig config);
    ~LlmClient() override = default;

    core::Message chatStream(
        const std::vector<core::Message>& messages,
        const nlohmann::json& toolsSpec,
        ChunkCallback onChunk
    ) const override;

private:
    void configureSession(cpr::Session& session,
                          const std::string& url,
                          const std::string& body) const;

    LlmConfig config_;
};

}  // namespace cpp_ai_agent::llm
