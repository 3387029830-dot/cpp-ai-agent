#include "core/Session.h"

#include <utility>

namespace cpp_ai_agent::core {

Session::Session(std::string id) : id_(std::move(id)) {}

void Session::addMessage(Message message) {
    messages_.push_back(std::move(message));
}

const std::vector<Message>& Session::messages() const {
    return messages_;
}

const std::string& Session::id() const {
    return id_;
}

}  // namespace cpp_ai_agent::core

