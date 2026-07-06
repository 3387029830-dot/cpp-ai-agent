#pragma once

#include "core/Message.h"

#include <string>
#include <vector>

namespace cpp_ai_agent::core {

class Session {
public:
    explicit Session(std::string id);

    void addMessage(Message message);
    const std::vector<Message>& messages() const;
    const std::string& id() const;

private:
    std::string id_;
    std::vector<Message> messages_;
};

}  // namespace cpp_ai_agent::core

