#include "agent/AgentLoop.h"
#include "config/AppConfig.h"
#include "core/Message.h"
#include "core/Session.h"
#include "diagnostics/Diagnostics.h"
#include "llm/LlmClient.h"
#include "security/PermissionManager.h"
#include "storage/HistoryReader.h"
#include "storage/JsonLogger.h"
#include "tools/FileTools.h"
#include "tools/ShellTool.h"
#include "tools/ToolRegistry.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
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

std::string envTemplateForProvider(const std::string& provider) {
    if (provider == "deepseek") {
        return
            "OPENAI_API_KEY=your_deepseek_key_here\n"
            "OPENAI_BASE_URL=https://api.deepseek.com\n"
            "OPENAI_MODEL=deepseek-chat\n"
            "OPENAI_PROXY_URL=\n";
    }

    if (provider == "linkapi") {
        return
            "OPENAI_API_KEY=your_linkapi_key_here\n"
            "OPENAI_BASE_URL=https://api.linkapi.ai/v1\n"
            "OPENAI_MODEL=gpt-5.4-mini\n"
            "OPENAI_PROXY_URL=\n";
    }

    return "";
}

int writeEnvTemplate(const std::string& provider) {
    const auto content = envTemplateForProvider(provider);
    if (content.empty()) {
        std::cout << "usage> ai-agent.exe /init-env deepseek|linkapi\n";
        return 1;
    }

    const auto envPath = std::filesystem::path(".env");
    if (std::filesystem::exists(envPath)) {
        std::cout << "init-env> .env already exists; not overwriting it.\n";
        std::cout << "hint> Edit .env manually, or rename/delete it before running /init-env.\n";
        return 1;
    }

    std::ofstream output(envPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cout << "init-env> failed to create .env\n";
        return 1;
    }

    output << content;
    std::cout << "init-env> wrote .env for " << provider << "\n";
    std::cout << "hint> Open .env and replace the placeholder API key.\n";
    return 0;
}

void printDemoGuide() {
    std::cout << "demo> cpp-ai-agent M5 demonstration\n\n";
    std::cout << "1. Build and test\n";
    std::cout << "   cmake --preset msvc-vcpkg-debug\n";
    std::cout << "   cmake --build --preset msvc-vcpkg-debug\n";
    std::cout << "   cd build\\msvc-vcpkg-debug\n";
    std::cout << "   ctest --output-on-failure\n\n";

    std::cout << "2. Configure provider\n";
    std::cout << "   .\\build\\msvc-vcpkg-debug\\ai-agent.exe /init-env deepseek\n";
    std::cout << "   notepad .env\n";
    std::cout << "   .\\build\\msvc-vcpkg-debug\\ai-agent.exe /doctor\n\n";

    std::cout << "3. Show read_file tool calling\n";
    std::cout << "   prompt: Read README.md and summarize this project in three bullet points.\n\n";

    std::cout << "4. Show permission control\n";
    std::cout << "   prompt: Create temp-demo.txt with the content hello agent.\n";
    std::cout << "   expected: permission prompt for write_file; type yes to allow.\n\n";

    std::cout << "5. Show history replay\n";
    std::cout << "   .\\build\\msvc-vcpkg-debug\\ai-agent.exe /history\n";
    std::cout << "   .\\build\\msvc-vcpkg-debug\\ai-agent.exe /replay logs\\session-xxxx.jsonl\n";
}

void printSearchPlaceholder() {
    std::cout << "search> Web search is a planned M5 extension point.\n";
    std::cout << "search> No search provider is configured in this demo build.\n";
    std::cout << "search> Extension idea: add a SearchTool implementing ITool and register it in ToolRegistry.\n";
}

void printSkills(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        std::cout << "skills> failed to open " << path.string() << "\n";
        return;
    }

    nlohmann::json json;
    input >> json;
    const auto skills = json.value("skills", nlohmann::json::array());
    if (skills.empty()) {
        std::cout << "skills> no skills configured.\n";
        std::cout << "skills> Add entries to config/skills.json to demonstrate prompt/tool presets.\n";
        return;
    }

    for (const auto& skill : skills) {
        std::cout << "skill> " << skill.value("name", "(unnamed)") << ": "
                  << skill.value("description", "") << "\n";
    }
}

void printMcpDemo() {
    std::cout << "mcp-demo> mock MCP client demonstration\n";
    std::cout << "mcp-demo> server: local-filesystem-mock\n";
    std::cout << "mcp-demo> tool: read_file\n";
    std::cout << "mcp-demo> request: {\"path\":\"README.md\"}\n";
    std::cout << "mcp-demo> response: routed to local ToolRegistry in this demo build\n";
    std::cout << "mcp-demo> boundary: real MCP transport is planned as a future extension.\n";
}

void printStatusUi(const cpp_ai_agent::config::AppConfig& config) {
    using namespace ftxui;

    auto document = window(
        text("cpp-ai-agent M5 status"),
        vbox({
            text("LLM: " + config.llm.model),
            text("Base URL: " + config.llm.baseUrl),
            text("Workspace: " + config.workspaceRoot),
            text("History: " + config.historyDir),
            separator(),
            text("Capabilities"),
            text("- Chat with OpenAI-compatible APIs"),
            text("- Tool calls: read/write/edit files, run commands"),
            text("- Permission confirmation and JSONL logs"),
            text("- History replay, config diagnostics, provider templates"),
        })
    );

    auto screen = Screen::Create(Dimension::Fit(document));
    Render(screen, document);
    screen.Print();
}

}  // namespace

int main(int argc, char* argv[]) {
    using cpp_ai_agent::config::loadAppConfig;
    using cpp_ai_agent::core::Role;
    using cpp_ai_agent::core::Session;
    using cpp_ai_agent::diagnostics::formatConfigSummary;
    using cpp_ai_agent::diagnostics::formatDiagnostics;
    using cpp_ai_agent::diagnostics::runLocalDiagnostics;
    using cpp_ai_agent::llm::LlmClient;
    using cpp_ai_agent::security::PermissionManager;
    using cpp_ai_agent::security::PermissionRequest;
    using cpp_ai_agent::storage::listHistoryFiles;
    using cpp_ai_agent::storage::JsonLogger;
    using cpp_ai_agent::storage::replayHistoryFile;
    using cpp_ai_agent::tools::EditFileTool;
    using cpp_ai_agent::tools::ReadFileTool;
    using cpp_ai_agent::tools::ShellTool;
    using cpp_ai_agent::tools::ToolRegistry;
    using cpp_ai_agent::tools::WriteFileTool;

    const auto consoleEncoding = configureConsoleEncoding();

    std::cout << "cpp-ai-agent M5 is running.\n";
    std::cout << "Type a message and press Enter. Type /exit to quit.\n\n";

    try {
        const auto appConfig = loadAppConfig("config/settings.json");

        if (argc >= 2) {
            const std::string command = argv[1];
            if (command == "/history") {
                const auto files = listHistoryFiles(std::filesystem::path(appConfig.historyDir));
                if (files.empty()) {
                    std::cout << "history> no logs found in " << appConfig.historyDir << "\n";
                    return 0;
                }

                for (const auto& file : files) {
                    std::cout << file.modifiedTime << "  " << file.size << " bytes  "
                              << file.path.string() << "\n";
                }
                return 0;
            }

            if (command == "/demo") {
                printDemoGuide();
                return 0;
            }

            if (command == "/ui") {
                printStatusUi(appConfig);
                return 0;
            }

            if (command == "/search") {
                printSearchPlaceholder();
                return 0;
            }

            if (command == "/skills") {
                printSkills("config/skills.json");
                return 0;
            }

            if (command == "/mcp-demo") {
                printMcpDemo();
                return 0;
            }

            if (command == "/init-env") {
                if (argc < 3) {
                    std::cout << "usage> ai-agent.exe /init-env deepseek|linkapi\n";
                    return 1;
                }
                return writeEnvTemplate(argv[2]);
            }

            if (command == "/config") {
                for (const auto& line : formatConfigSummary(appConfig)) {
                    std::cout << line << "\n";
                }
                return 0;
            }

            if (command == "/doctor") {
                for (const auto& line : formatDiagnostics(runLocalDiagnostics(appConfig))) {
                    std::cout << line << "\n";
                }
                return 0;
            }

            if (command == "/replay") {
                if (argc < 3) {
                    std::cout << "usage> ai-agent.exe /replay logs\\session-*.jsonl\n";
                    return 1;
                }

                for (const auto& line : replayHistoryFile(std::filesystem::path(argv[2]))) {
                    std::cout << line << "\n";
                }
                return 0;
            }
        }

        LlmClient llm(appConfig.llm);
        ToolRegistry tools;
        tools.registerTool(std::make_shared<ReadFileTool>(std::filesystem::path(appConfig.workspaceRoot)));
        tools.registerTool(std::make_shared<WriteFileTool>(std::filesystem::path(appConfig.workspaceRoot)));
        tools.registerTool(std::make_shared<EditFileTool>(std::filesystem::path(appConfig.workspaceRoot)));
        tools.registerTool(std::make_shared<ShellTool>());

        PermissionManager permissions(
            appConfig.permissionMode,
            [](const PermissionRequest& request) {
                std::cout << "\npermission> allow tool '" << request.toolName << "' (risk="
                          << cpp_ai_agent::security::riskLevelToString(request.risk) << ")?\n";
                std::cout << "arguments> " << request.arguments << "\n";
                std::cout << "type yes to allow> ";

                std::string answer;
                if (!std::getline(std::cin, answer)) {
                    return false;
                }

                return answer == "yes" || answer == "y";
            }
        );
        JsonLogger logger(std::filesystem::path(appConfig.historyDir));
        std::cout << "log> " << logger.path().string() << "\n";

        cpp_ai_agent::agent::AgentLoop agentLoop(
            llm,
            tools,
            permissions,
            logger,
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
            logger.log("user_message", {{"content", input}});

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
