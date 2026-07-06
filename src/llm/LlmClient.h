#pragma once

#include "core/Message.h"

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

private:
    LlmConfig config_;
};

}  // namespace cpp_ai_agent::llm
