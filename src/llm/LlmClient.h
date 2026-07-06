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

class LlmClient {
public:
    explicit LlmClient(LlmConfig config);

    core::Message chat(const std::vector<core::Message>& messages) const;
    core::Message chat(
        const std::vector<core::Message>& messages,
        const nlohmann::json& toolsSpec
    ) const;

private:
    LlmConfig config_;
};

}  // namespace cpp_ai_agent::llm
