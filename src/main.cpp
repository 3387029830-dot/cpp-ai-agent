#include "config/AppConfig.h"
#include "core/Message.h"
#include "core/Session.h"
#include "llm/LlmClient.h"

#include <exception>
#include <iostream>
#include <string>

namespace {

cpp_ai_agent::core::Message makeMessage(cpp_ai_agent::core::Role role, std::string content) {
    cpp_ai_agent::core::Message message;
    message.role = role;
    message.content = std::move(content);
    return message;
}

}  // namespace

int main() {
    using cpp_ai_agent::config::loadAppConfig;
    using cpp_ai_agent::core::Role;
    using cpp_ai_agent::core::Session;
    using cpp_ai_agent::llm::LlmClient;

    std::cout << "cpp-ai-agent M1 is running.\n";
    std::cout << "Type a message and press Enter. Type /exit to quit.\n\n";

    try {
        const auto appConfig = loadAppConfig("config/settings.json");
        LlmClient llm(appConfig.llm);
        Session session("default");

        session.addMessage(makeMessage(
            Role::System,
            "You are cpp-ai-agent, a concise AI coding assistant running in a C++ terminal app."
        ));

        std::string input;
        while (true) {
            std::cout << "user> ";

            if (!std::getline(std::cin, input)) {
                std::cout << "\n";
                break;
            }

            if (input == "/exit") {
                break;
            }

            if (input.empty()) {
                continue;
            }

            session.addMessage(makeMessage(Role::User, input));

            try {
                const auto reply = llm.chat(session.messages());
                session.addMessage(reply);
                std::cout << "assistant> " << reply.content << "\n";
            } catch (const std::exception& ex) {
                std::cout << "error> " << ex.what() << "\n";
                std::cout << "hint> Check OPENAI_API_KEY, config/settings.json, and network access.\n";
            }
        }
    } catch (const std::exception& ex) {
        std::cout << "startup error> " << ex.what() << "\n";
        std::cout << "hint> Set OPENAI_API_KEY before starting the program.\n";
        std::cout << "      PowerShell example: $env:OPENAI_API_KEY='your_key'\n";
        return 1;
    }

    std::cout << "bye\n";
    return 0;
}

