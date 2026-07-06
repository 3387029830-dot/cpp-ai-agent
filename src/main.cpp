#include "agent/AgentLoop.h"
#include "config/AppConfig.h"
#include "core/Message.h"
#include "core/Session.h"
#include "llm/LlmClient.h"
#include "tools/FileTools.h"
#include "tools/ToolRegistry.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

struct ConsoleEncoding {
    unsigned int inputCodePage = 0;
    bool stdinIsConsole = false;
};

cpp_ai_agent::core::Message makeMessage(cpp_ai_agent::core::Role role, std::string content) {
    cpp_ai_agent::core::Message message;
    message.role = role;
    message.content = std::move(content);
    return message;
}

#ifdef _WIN32
ConsoleEncoding configureConsoleEncoding() {
    DWORD consoleMode = 0;
    ConsoleEncoding encoding;
    encoding.stdinIsConsole = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &consoleMode) != 0;
    encoding.inputCodePage = GetConsoleCP();
    SetConsoleOutputCP(CP_UTF8);
    if (encoding.inputCodePage == 0) {
        encoding.inputCodePage = GetACP();
    }
    return encoding;
}

bool isValidUtf8(const std::string& value) {
    if (value.empty()) {
        return true;
    }

    return MultiByteToWideChar(
               CP_UTF8,
               MB_ERR_INVALID_CHARS,
               value.data(),
               static_cast<int>(value.size()),
               nullptr,
               0
           ) > 0;
}

std::string convertToUtf8(const std::string& value, ConsoleEncoding encoding) {
    if (value.empty()) {
        return value;
    }

    if (!encoding.stdinIsConsole && isValidUtf8(value)) {
        return value;
    }

    if (encoding.stdinIsConsole && encoding.inputCodePage == CP_UTF8 && isValidUtf8(value)) {
        return value;
    }

    const auto wideLength = MultiByteToWideChar(
        encoding.inputCodePage,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );
    if (wideLength <= 0) {
        return value;
    }

    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    MultiByteToWideChar(
        encoding.inputCodePage,
        0,
        value.data(),
        static_cast<int>(value.size()),
        wide.data(),
        wideLength
    );

    const auto utf8Length = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.data(),
        wideLength,
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (utf8Length <= 0) {
        return value;
    }

    std::string utf8(static_cast<std::size_t>(utf8Length), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.data(),
        wideLength,
        utf8.data(),
        utf8Length,
        nullptr,
        nullptr
    );
    return utf8;
}
#else
struct ConsoleEncoding {};

ConsoleEncoding configureConsoleEncoding() {
    return {};
}

std::string convertToUtf8(const std::string& value, ConsoleEncoding) {
    return value;
}
#endif

}  // namespace

int main() {
    using cpp_ai_agent::config::loadAppConfig;
    using cpp_ai_agent::core::Role;
    using cpp_ai_agent::core::Session;
    using cpp_ai_agent::llm::LlmClient;
    using cpp_ai_agent::tools::ReadFileTool;
    using cpp_ai_agent::tools::ToolRegistry;

    const auto consoleEncoding = configureConsoleEncoding();

    std::cout << "cpp-ai-agent M3 is running.\n";
    std::cout << "Type a message and press Enter. Type /exit to quit.\n\n";

    try {
        const auto appConfig = loadAppConfig("config/settings.json");
        LlmClient llm(appConfig.llm);
        ToolRegistry tools;
        tools.registerTool(std::make_shared<ReadFileTool>(std::filesystem::path(appConfig.workspaceRoot)));

        cpp_ai_agent::agent::AgentLoop agentLoop(
            llm,
            tools,
            appConfig.maxIterations,
            [](const cpp_ai_agent::agent::AgentEvent& event) {
                using cpp_ai_agent::agent::AgentEventType;

                if (event.type == AgentEventType::ToolCall) {
                    std::cout << "tool_call> " << event.title << " " << event.detail << "\n";
                } else if (event.type == AgentEventType::ToolResult) {
                    std::cout << "tool_result> " << event.title << ": " << event.detail << "\n";
                } else if (event.type == AgentEventType::Warning) {
                    std::cout << "warning> " << event.detail << "\n";
                }
            }
        );

        Session session("default");

        session.addMessage(makeMessage(
            Role::System,
            "You are cpp-ai-agent, a concise AI coding assistant running in a C++ terminal app. "
            "When the user asks you to inspect, read, or summarize a local project file, use the "
            "read_file tool instead of guessing."
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

            input = convertToUtf8(input, consoleEncoding);
            session.addMessage(makeMessage(Role::User, input));

            try {
                const auto reply = agentLoop.runTurn(session);
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
