#pragma once

#include "core/Message.h"

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <vector>

namespace cpp_ai_agent::llm {

struct LlmConfig {
    std::string baseUrl;
    std::string apiKey;
    std::string model;
    std::string proxyUrl;
};

class ILlmClient {
public:
    virtual ~ILlmClient() = default;

    virtual core::Message chat(const std::vector<core::Message>& messages) const = 0;
    virtual core::Message chat(
        const std::vector<core::Message>& messages,
        const nlohmann::json& toolsSpec
    ) const = 0;
};

class LlmClient : public ILlmClient {
public:
    explicit LlmClient(LlmConfig config);
    ~LlmClient() override = default;

    core::Message chat(const std::vector<core::Message>& messages) const override;
    core::Message chat(
        const std::vector<core::Message>& messages,
        const nlohmann::json& toolsSpec
    ) const override;

private:
    LlmConfig config_;
};

}  // namespace cpp_ai_agent::llm
