#include "agent/AgentLoop.h"
#include "config/AppConfig.h"
#include "core/Message.h"
#include "core/Session.h"
#include "diagnostics/Diagnostics.h"
#include "llm/LlmClient.h"
#include "mcp/McpClient.h"
#include "mcp/McpToolAdapter.h"
#include "security/PermissionManager.h"
#include "skills/SkillCatalog.h"
#include "storage/HistoryReader.h"
#include "storage/JsonLogger.h"
#include "tools/FileTools.h"
#include "tools/ShellTool.h"
#include "tools/ToolRegistry.h"
#include "tools/WebSearchTool.h"
#include "ui/Console.h"
#include "utils/Encoding.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
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
std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const auto utf8Length = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (utf8Length <= 0) {
        return {};
    }

    std::string utf8(static_cast<std::size_t>(utf8Length), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        utf8.data(),
        utf8Length,
        nullptr,
        nullptr
    );
    return utf8;
}

std::vector<std::string> commandLineArgs(int argc, char* argv[]) {
    int wideArgc = 0;
    LPWSTR* wideArgv = CommandLineToArgvW(GetCommandLineW(), &wideArgc);
    if (wideArgv == nullptr) {
        std::vector<std::string> fallback;
        for (int i = 0; i < argc; ++i) {
            fallback.emplace_back(argv[i]);
        }
        return fallback;
    }

    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(wideArgc));
    for (int i = 0; i < wideArgc; ++i) {
        args.push_back(wideToUtf8(wideArgv[i]));
    }
    LocalFree(wideArgv);
    return args;
}

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
    return cpp_ai_agent::utils::isValidUtf8(value);
}

std::string convertToUtf8(const std::string& value, ConsoleEncoding encoding) {
    if (value.empty()) {
        return value;
    }

    if (!encoding.stdinIsConsole && cpp_ai_agent::utils::isValidUtf8(value)) {
        return value;
    }

    if (encoding.stdinIsConsole && encoding.inputCodePage == CP_UTF8 && cpp_ai_agent::utils::isValidUtf8(value)) {
        return value;
    }

    return cpp_ai_agent::utils::toUtf8(value, encoding.inputCodePage);
}
#else
struct ConsoleEncoding {};

std::vector<std::string> commandLineArgs(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return args;
}

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
            "OPENAI_PROXY_URL=\n"
            "WEB_SEARCH_PROXY_URL=\n";
    }

    if (provider == "linkapi") {
        return
            "OPENAI_API_KEY=your_linkapi_key_here\n"
            "OPENAI_BASE_URL=https://api.linkapi.ai/v1\n"
            "OPENAI_MODEL=gpt-5.4-mini\n"
            "OPENAI_PROXY_URL=\n"
            "WEB_SEARCH_PROXY_URL=\n";
    }

    return "";
}

std::string trimCommandInput(std::string value) {
    const std::string utf8Bom = "\xEF\xBB\xBF";
    if (value.rfind(utf8Bom, 0) == 0) {
        value.erase(0, utf8Bom.size());
    }

    const auto isSpace = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), isSpace));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), isSpace).base(), value.end());
    return value;
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
    std::cout << "   ctest -C Debug --output-on-failure\n\n";

    std::cout << "2. Configure provider\n";
    std::cout << "   .\\build\\msvc-vcpkg-debug\\ai-agent.exe /init-env deepseek\n";
    std::cout << "   notepad .env\n";
    std::cout << "   .\\build\\msvc-vcpkg-debug\\ai-agent.exe /doctor\n\n";

    std::cout << "3. Show read_file tool calling\n";
    std::cout << "   prompt: Read README.md and summarize this project in three bullet points.\n\n";

    std::cout << "4. Show permission control\n";
    std::cout << "   prompt: Create temp-demo.txt with the content hello agent.\n";
    std::cout << "   expected: permission prompt for write_file; choose an option with arrow keys and press Enter.\n\n";

    std::cout << "5. Show history replay\n";
    std::cout << "   .\\build\\msvc-vcpkg-debug\\ai-agent.exe /history\n";
    std::cout << "   .\\build\\msvc-vcpkg-debug\\ai-agent.exe /replay logs\\session-xxxx.jsonl\n";
}

void printSearchPlaceholder() {
    std::cout << "usage> ai-agent.exe /search <query>\n";
    std::cout << "example> ai-agent.exe /search cpp-ai-agent MCP stdio\n";
}

void printCommandPalette() {
    std::cout << "commands> available slash commands\n";
    std::cout << "\n";
    std::cout << "commands> chat commands\n";
    std::cout << "  /help /? /commands\n";
    std::cout << "                   Show this command palette\n";
    std::cout << "  /skills          List configured Skills\n";
    std::cout << "  /use-skill NAME [target]\n";
    std::cout << "                   Activate a Skill for this chat\n";
    std::cout << "  /clear-skill     Clear the active Skill\n";
    std::cout << "  /load [path]     Resume a previous JSONL session log\n";
    std::cout << "  /exit            Exit the chat\n\n";

    std::cout << "commands> standalone demo commands\n";
    std::cout << "  ai-agent.exe /\n";
    std::cout << "  ai-agent.exe /?\n";
    std::cout << "  ai-agent.exe /help\n";
    std::cout << "  ai-agent.exe /commands\n";
    std::cout << "  ai-agent.exe /config\n";
    std::cout << "  ai-agent.exe /doctor\n";
    std::cout << "  ai-agent.exe /demo\n";
    std::cout << "  ai-agent.exe /ui\n";
    std::cout << "  ai-agent.exe /search <query>\n";
    std::cout << "  ai-agent.exe /skills\n";
    std::cout << "  ai-agent.exe /use-skill\n";
    std::cout << "  ai-agent.exe /mcp\n";
    std::cout << "  ai-agent.exe /mcp-demo\n";
    std::cout << "  ai-agent.exe /mcp-call-demo\n";
    std::cout << "  ai-agent.exe /mcp-connect <command> [args...]\n";
    std::cout << "  ai-agent.exe /history\n";
    std::cout << "  ai-agent.exe /load <log.jsonl>\n";
    std::cout << "  ai-agent.exe /replay <log.jsonl>\n";
    std::cout << "  ai-agent.exe /init-env deepseek|linkapi\n\n";

    std::cout << "commands> internal command\n";
    std::cout << "  ai-agent.exe /mcp-test-server\n";
    std::cout << "                   Internal stdio MCP server used by /mcp-demo and /mcp-call-demo\n\n";

    std::cout << "commands> tip: type /use-skill code_review src/mcp to show Skill mode.\n\n";
}

void printSkills(const cpp_ai_agent::skills::SkillCatalog& catalog) {
    if (catalog.all().empty()) {
        std::cout << "skills> no skills configured.\n";
        std::cout << "skills> Add entries to config/skills.json to demonstrate prompt/tool presets.\n";
        return;
    }

    std::cout << "skills> available skills\n";
    for (const auto& skill : catalog.all()) {
        std::cout << "  - " << skill.name << ": " << skill.description << "\n";
        if (!skill.allowedTools.empty()) {
            std::cout << "    tools: ";
            for (std::size_t i = 0; i < skill.allowedTools.size(); ++i) {
                if (i > 0) {
                    std::cout << ", ";
                }
                std::cout << skill.allowedTools.at(i);
            }
            std::cout << "\n";
        }
        if (!skill.suggestedPrompt.empty()) {
            std::cout << "    prompt: " << skill.suggestedPrompt << "\n";
        }
    }
    std::cout << "skills> use /use-skill <name> [target] in chat to activate one.\n";
}

void printMcpServerInfo(const cpp_ai_agent::mcp::McpServerInfo& info) {
    std::cout << "mcp> connected\n";
    std::cout << "mcp> protocol: " << info.protocolVersion << "\n";
    std::cout << "mcp> server: " << info.name << " " << info.version << "\n";
    if (!info.instructions.empty()) {
        std::cout << "mcp> instructions: " << info.instructions << "\n";
    }

    if (info.tools.empty()) {
        std::cout << "mcp> tools: (none)\n";
        return;
    }

    std::cout << "mcp> tools:\n";
    for (const auto& tool : info.tools) {
        std::cout << "  - " << tool.name;
        if (!tool.description.empty()) {
            std::cout << ": " << tool.description;
        }
        std::cout << "\n";
    }
}

void printMcpDemo(const std::string& executablePath) {
    std::cout << "mcp-demo> starting built-in stdio MCP test server\n";
    cpp_ai_agent::mcp::StdioMcpClient client({executablePath, "/mcp-test-server"});
    printMcpServerInfo(client.discoverTools());
    std::cout << "mcp-demo> this is a real initialize + tools/list stdio handshake\n";
    std::cout << "mcp-demo> use /mcp-connect <command> [args...] to inspect an external stdio MCP server\n";
}

void printMcpCallDemo(const std::string& executablePath) {
    std::cout << "mcp-call-demo> starting built-in stdio MCP test server\n";
    cpp_ai_agent::mcp::StdioMcpClient client({executablePath, "/mcp-test-server"});
    const auto echo = client.callTool("echo", {{"text", "hello from cpp-ai-agent"}});
    std::cout << "mcp-call-demo> echo => " << echo.text << "\n";

    const auto projectInfo = client.callTool("project_info", nlohmann::json::object());
    std::cout << "mcp-call-demo> project_info => " << projectInfo.text << "\n";
    std::cout << "mcp-call-demo> this is a real tools/call round trip\n";
}

void registerBuiltInMcpTools(
    cpp_ai_agent::tools::ToolRegistry& tools,
    const std::string& executablePath
) {
    const std::vector<std::string> command = {executablePath, "/mcp-test-server"};
    cpp_ai_agent::mcp::StdioMcpClient client(command);
    const auto info = client.discoverTools();

    for (const auto& tool : info.tools) {
        tools.registerTool(std::make_shared<cpp_ai_agent::mcp::McpToolAdapter>(
            command,
            "mcp_" + tool.name,
            tool
        ));
    }
}

void registerConfiguredMcpTools(
    cpp_ai_agent::tools::ToolRegistry& tools,
    const std::filesystem::path& configPath
) {
    if (!std::filesystem::exists(configPath)) {
        return;
    }

    std::ifstream input(configPath);
    if (!input) {
        std::cout << "warning> failed to open " << configPath.string() << "\n";
        return;
    }

    const auto config = nlohmann::json::parse(input);
    for (const auto& server : config.value("servers", nlohmann::json::array())) {
        if (!server.value("enabled", false)) {
            continue;
        }

        const auto serverName = server.value("name", "");
        const auto command = server.value("command", "");
        if (serverName.empty() || command.empty()) {
            std::cout << "warning> skipped MCP server with missing name or command\n";
            continue;
        }

        std::vector<std::string> mcpCommand = {command};
        for (const auto& arg : server.value("args", nlohmann::json::array())) {
            mcpCommand.push_back(arg.get<std::string>());
        }

        try {
            cpp_ai_agent::mcp::StdioMcpClient client(mcpCommand);
            const auto info = client.discoverTools();
            for (const auto& tool : info.tools) {
                tools.registerTool(std::make_shared<cpp_ai_agent::mcp::McpToolAdapter>(
                    mcpCommand,
                    cpp_ai_agent::mcp::makeMcpToolName(serverName, tool.name),
                    tool
                ));
            }
            std::cout << "mcp> registered " << info.tools.size() << " tools from " << serverName << "\n";
        } catch (const std::exception& ex) {
            std::cout << "warning> MCP server '" << serverName << "' unavailable: " << ex.what() << "\n";
        }
    }
}

int runMcpTestServer() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        const auto request = nlohmann::json::parse(line);
        const auto method = request.value("method", "");
        if (!request.contains("id")) {
            continue;
        }

        const auto id = request.at("id");
        if (method == "initialize") {
            nlohmann::json response = {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result",
                 {
                     {"protocolVersion", "2025-03-26"},
                     {"capabilities", {{"tools", {{"listChanged", false}}}}},
                     {"serverInfo", {{"name", "cpp-ai-agent-test-mcp"}, {"version", "0.1.0"}}},
                     {"instructions", "Built-in MCP test server for cpp-ai-agent demos."},
                 }},
            };
            std::cout << response.dump() << std::endl;
        } else if (method == "tools/list") {
            nlohmann::json response = {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result",
                 {
                     {"tools",
                      nlohmann::json::array({
                          {
                              {"name", "echo"},
                              {"description", "Echo text back to the caller."},
                              {"inputSchema",
                               {
                                   {"type", "object"},
                                   {"properties", {{"text", {{"type", "string"}}}}},
                                   {"required", nlohmann::json::array({"text"})},
                               }},
                          },
                          {
                              {"name", "project_info"},
                              {"description", "Return basic information about cpp-ai-agent."},
                              {"inputSchema", {{"type", "object"}, {"properties", nlohmann::json::object()}}},
                          },
                      })},
                 }},
            };
            std::cout << response.dump() << std::endl;
        } else if (method == "tools/call") {
            const auto params = request.value("params", nlohmann::json::object());
            const auto toolName = params.value("name", "");
            const auto arguments = params.value("arguments", nlohmann::json::object());

            std::string text;
            bool isError = false;
            if (toolName == "echo") {
                text = arguments.value("text", "");
            } else if (toolName == "project_info") {
                text = "cpp-ai-agent: C++17 terminal AI coding agent with local tools and MCP demo support.";
            } else {
                isError = true;
                text = "Unknown tool: " + toolName;
            }

            nlohmann::json response = {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result",
                 {
                     {"content", nlohmann::json::array({{{"type", "text"}, {"text", text}}})},
                     {"isError", isError},
                 }},
            };
            std::cout << response.dump() << std::endl;
        } else {
            nlohmann::json response = {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"error", {{"code", -32601}, {"message", "Method not found"}}},
            };
            std::cout << response.dump() << std::endl;
        }
    }

    return 0;
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
    using cpp_ai_agent::skills::loadSkillCatalog;
    using cpp_ai_agent::skills::makeSkillSystemMessage;
    using cpp_ai_agent::storage::listHistoryFiles;
    using cpp_ai_agent::storage::loadMessagesFromHistory;
    using cpp_ai_agent::storage::JsonLogger;
    using cpp_ai_agent::storage::replayHistoryFile;
    using cpp_ai_agent::tools::EditFileTool;
    using cpp_ai_agent::tools::ListDirTool;
    using cpp_ai_agent::tools::ReadFileTool;
    using cpp_ai_agent::tools::ShellTool;
    using cpp_ai_agent::tools::ToolRegistry;
    using cpp_ai_agent::tools::WebSearchTool;
    using cpp_ai_agent::tools::WriteFileTool;
    using cpp_ai_agent::ui::Console;
    using cpp_ai_agent::ui::detectColorSupport;
    using cpp_ai_agent::ui::detectInteractiveOutput;

    const auto consoleEncoding = configureConsoleEncoding();
    Console console(detectColorSupport(), detectInteractiveOutput());
    const auto args = commandLineArgs(argc, argv);
    const auto argCount = static_cast<int>(args.size());

    try {
        const auto appConfig = loadAppConfig("config/settings.json");
        const auto skillCatalog = loadSkillCatalog("config/skills.json");

        // When /load is used as a CLI argument, history is parsed here
        // and the messages are injected into the session below.
        std::vector<cpp_ai_agent::core::Message> preloadedMessages;
        std::string preloadedSource;

        if (argCount >= 2) {
            const std::string command = args.at(1);
            if (command == "/" || command == "/?" || command == "/help" || command == "/commands") {
                printCommandPalette();
                return 0;
            }

            if (command == "/history") {
                const auto files = listHistoryFiles(std::filesystem::path(appConfig.historyDir));
                if (files.empty()) {
                    std::cout << "history> no logs found in " << appConfig.historyDir << "\n";
                    return 0;
                }

                for (const auto& file : files) {
                    std::cout << file.modifiedTime << "  "
                              << std::setw(8) << file.size << " bytes  ";
                    if (!file.title.empty()) {
                        std::cout << "[" << file.title << "]  ";
                    }
                    std::cout << file.path.string() << "\n";
                }
                return 0;
            }

            if (command == "/demo") {
                printDemoGuide();
                return 0;
            }

            if (command == "/ui") {
                console.printUiOverview(
                    appConfig.llm.model,
                    appConfig.llm.baseUrl,
                    appConfig.workspaceRoot,
                    appConfig.historyDir
                );
                return 0;
            }

            if (command == "/search") {
                if (argCount < 3) {
                    printSearchPlaceholder();
                    return 1;
                }
                std::string query;
                for (int i = 2; i < argCount; ++i) {
                    if (!query.empty()) {
                        query.push_back(' ');
                    }
                    query += args.at(static_cast<std::size_t>(i));
                }
                WebSearchTool searchTool(appConfig.webSearchProxyUrl);
                const auto result = searchTool.execute({{"query", query}, {"max_results", 5}});
                std::cout << (result.success ? result.output : "search> " + result.output + "\n");
                return 0;
            }

            if (command == "/skills") {
                printSkills(skillCatalog);
                return 0;
            }

            if (command == "/use-skill") {
                std::cout << "usage> ai-agent.exe\n";
                std::cout << "then type> /use-skill <name> [target]\n";
                printSkills(skillCatalog);
                return 0;
            }

            if (command == "/mcp-demo") {
                printMcpDemo(args.at(0));
                return 0;
            }

            if (command == "/mcp-call-demo") {
                printMcpCallDemo(args.at(0));
                return 0;
            }

            if (command == "/mcp-connect") {
                if (argCount < 3) {
                    std::cout << "usage> ai-agent.exe /mcp-connect <command> [args...]\n";
                    std::cout << "example> ai-agent.exe /mcp-connect npx -y @modelcontextprotocol/server-filesystem .\n";
                    return 1;
                }

                std::vector<std::string> mcpCommand;
                for (int i = 2; i < argCount; ++i) {
                    mcpCommand.emplace_back(args.at(static_cast<std::size_t>(i)));
                }
                cpp_ai_agent::mcp::StdioMcpClient client(std::move(mcpCommand));
                printMcpServerInfo(client.discoverTools());
                return 0;
            }

            if (command == "/mcp-test-server") {
                return runMcpTestServer();
            }

            if (command == "/mcp") {
                std::cout << "usage> ai-agent.exe /mcp-demo\n";
                std::cout << "usage> ai-agent.exe /mcp-call-demo\n";
                std::cout << "usage> ai-agent.exe /mcp-connect <command> [args...]\n";
                return 0;
            }

            if (command == "/init-env") {
                if (argCount < 3) {
                    std::cout << "usage> ai-agent.exe /init-env deepseek|linkapi\n";
                    return 1;
                }
                return writeEnvTemplate(args.at(2));
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
                if (argCount < 3) {
                    std::cout << "usage> ai-agent.exe /replay logs\\session-*.jsonl\n";
                    return 1;
                }

                for (const auto& line : replayHistoryFile(std::filesystem::path(args.at(2)))) {
                    std::cout << line << "\n";
                }
                return 0;
            }

            if (command == "/load") {
                if (argCount < 3) {
                    std::cout << "usage> ai-agent.exe /load logs\\session-*.jsonl\n";
                    return 1;
                }
                // Load messages now and fall through to the REPL.
                const std::filesystem::path loadPath(args.at(2));
                int skipped = 0;
                preloadedMessages = loadMessagesFromHistory(loadPath, &skipped);
                preloadedSource = loadPath.filename().string();
                std::cout << "loaded> " << preloadedSource
                          << " (" << preloadedMessages.size() << " messages)";
                if (skipped > 0) {
                    std::cout << ", skipped " << skipped << " corrupted lines";
                }
                std::cout << "\n\n";
                // Fall through — do NOT return.
            }
        }

        LlmClient llm(appConfig.llm);
        ToolRegistry tools;
        tools.registerTool(std::make_shared<ListDirTool>(std::filesystem::path(appConfig.workspaceRoot)));
        tools.registerTool(std::make_shared<ReadFileTool>(std::filesystem::path(appConfig.workspaceRoot)));
        tools.registerTool(std::make_shared<WriteFileTool>(std::filesystem::path(appConfig.workspaceRoot)));
        tools.registerTool(std::make_shared<EditFileTool>(std::filesystem::path(appConfig.workspaceRoot)));
        tools.registerTool(std::make_shared<ShellTool>());
        tools.registerTool(std::make_shared<WebSearchTool>(appConfig.webSearchProxyUrl));
        registerBuiltInMcpTools(tools, args.at(0));
        registerConfiguredMcpTools(tools, "config/mcp_servers.json");

        PermissionManager permissions(
            appConfig.permissionMode,
            [&console](const PermissionRequest& request) {
                return console.confirmPermission(
                    request.toolName,
                    cpp_ai_agent::security::riskLevelToString(request.risk),
                    request.arguments,
                    request.preview
                );
            }
        );
        JsonLogger logger(std::filesystem::path(appConfig.historyDir));
        console.printBanner(appConfig.llm.model, appConfig.workspaceRoot);
        std::cout << "log> " << logger.path().string() << "\n\n";

        std::string activeSkillName;
        std::string activeSkillTarget;
        std::vector<std::string> activeAllowedTools;

        cpp_ai_agent::agent::AgentLoop agentLoop(
            llm,
            tools,
            permissions,
            logger,
            appConfig.maxIterations,
            [&console, &tools](const cpp_ai_agent::agent::AgentEvent& event) {
                using cpp_ai_agent::agent::AgentEventType;

                if (event.type == AgentEventType::AssistantChunk) {
                    console.printAssistantChunk(event.detail);
                } else if (event.type == AgentEventType::AssistantDone) {
                    console.finishAssistantStream();
                } else if (event.type == AgentEventType::AssistantMessage) {
                    console.printAssistant(event.detail);
                } else if (event.type == AgentEventType::ToolCall) {
                    const auto* tool = tools.find(event.title);
                    const auto risk =
                        tool == nullptr ? "unknown" : cpp_ai_agent::security::riskLevelToString(tool->risk());
                    console.printToolCall(event.title, event.detail, risk);
                } else if (event.type == AgentEventType::ToolResult) {
                    console.printToolResult(event.title, event.detail);
                } else if (event.type == AgentEventType::Warning) {
                    console.printWarning(event.detail);
                }
            },
            8000,
            [&activeSkillName, &activeAllowedTools](const std::string& toolName) {
                if (activeSkillName.empty() || activeAllowedTools.empty()) {
                    return std::string();
                }
                if (std::find(activeAllowedTools.begin(), activeAllowedTools.end(), toolName) != activeAllowedTools.end()) {
                    return std::string();
                }
                return "Tool '" + toolName + "' is blocked by active skill '" + activeSkillName + "'.";
            }
        );

        // -- 加载项目规范文件 AGENTS.md --
        auto loadAgentsMd = [](const std::filesystem::path& workspaceRoot) -> std::string {
            const auto agentsPath = workspaceRoot / "AGENTS.md";
            if (!std::filesystem::exists(agentsPath)) {
                return "";
            }
            std::ifstream input(agentsPath);
            if (!input) {
                return "";
            }
            std::ostringstream oss;
            oss << input.rdbuf();
            return oss.str();
        };
        const std::string agentsMdContent = loadAgentsMd(std::filesystem::path(appConfig.workspaceRoot));

        Session session("default");

        std::string systemPrompt =
            "You are cpp-ai-agent, a concise AI coding assistant running in a C++ terminal app.\n"
            "\n"
            "=== TOOL USAGE RULES ===\n"
            "- When the user gives you a specific file path or name, call read_file directly — do not list_dir first.\n"
            "- Only use list_dir when the user asks what files exist or tells you to explore a directory.\n"
            "- The mcp_project_info tool is a demo tool; do not call it during real tasks.\n"
            "\n"
            "=== CLARIFICATION RULES ===\n"
            "- If the user's message is vague, ambiguous, or too short to understand (e.g. just 'OK', '好的', 'yes', 'do it'), "
            "DO NOT simply echo back or guess. Instead, ask 1–2 specific clarifying questions to narrow down what they want.\n"
            "- If the user seems to agree to something ('好的', 'OK', 'go ahead') but you cannot see the original proposal "
            "in the recent messages, respond with: '我没有看到之前的上下文，能再说一下你想要我做什么吗？'\n"
            "- When asking clarifying questions, be specific and offer concrete options if possible. "
            "For example: '你是想让我审查代码、写测试、还是总结项目？'\n"
            "- Once the user clarifies, proceed directly with the task without further confirmation.";

        if (!agentsMdContent.empty()) {
            systemPrompt += "\n\n=== PROJECT RULES (AGENTS.md) ===\n";
            systemPrompt += "The following project-specific conventions override general behavior when applicable.\n";
            systemPrompt += agentsMdContent;
        }

        session.addMessage(makeMessage(Role::System, systemPrompt));

        // Inject messages loaded via CLI /load, if any.
        if (!preloadedMessages.empty()) {
            for (const auto& msg : preloadedMessages) {
                session.addMessage(msg);
            }
        }

        bool firstUserMessage = preloadedMessages.empty();
        std::string input;
        while (true) {
            std::cout << "\033[36m▸ you\033[0m ";

            if (!std::getline(std::cin, input)) {
                std::cout << "\n";
                break;
            }

            input = convertToUtf8(input, consoleEncoding);
            const auto commandInput = trimCommandInput(input);

            if (commandInput == "/exit") {
                break;
            }

            if (commandInput.empty()) {
                continue;
            }

            if (commandInput == "/" || commandInput == "/?" || commandInput == "/help" || commandInput == "/commands") {
                printCommandPalette();
                continue;
            }

            if (commandInput == "/skills") {
                printSkills(skillCatalog);
                continue;
            }

            const std::string useSkillPrefix = "/use-skill ";
            if (commandInput.rfind(useSkillPrefix, 0) == 0) {
                auto rest = trimCommandInput(commandInput.substr(useSkillPrefix.size()));
                const auto firstSpace = rest.find(' ');
                const auto skillName = firstSpace == std::string::npos ? rest : rest.substr(0, firstSpace);
                const auto skillTarget = firstSpace == std::string::npos ? "" : trimCommandInput(rest.substr(firstSpace + 1));
                const auto* skill = skillCatalog.find(skillName);
                if (skill == nullptr) {
                    console.printWarning("Unknown skill: " + skillName);
                    printSkills(skillCatalog);
                    continue;
                }

                activeSkillName = skill->name;
                activeSkillTarget = skillTarget;
                activeAllowedTools = skill->allowedTools;
                session.addMessage(makeMessage(Role::System, makeSkillSystemMessage(*skill, activeSkillTarget)));
                logger.log(
                    "skill_selected",
                    {
                        {"name", skill->name},
                        {"description", skill->description},
                        {"target", activeSkillTarget},
                        {"allowed_tools", activeAllowedTools},
                    }
                );
                std::cout << "skill> active: " << skill->name << "\n";
                if (!activeSkillTarget.empty()) {
                    std::cout << "skill> target: " << activeSkillTarget << "\n";
                }
                if (!activeAllowedTools.empty()) {
                    std::cout << "skill> allowed tools: ";
                    for (std::size_t i = 0; i < activeAllowedTools.size(); ++i) {
                        if (i > 0) {
                            std::cout << ", ";
                        }
                        std::cout << activeAllowedTools.at(i);
                    }
                    std::cout << "\n";
                }
                if (!skill->suggestedPrompt.empty()) {
                    std::cout << "skill> suggested prompt: " << skill->suggestedPrompt << "\n";
                }
                std::cout << "\n";
                continue;
            }

            if (commandInput == "/clear-skill") {
                if (activeSkillName.empty()) {
                    std::cout << "skill> no active skill to clear.\n\n";
                } else {
                    std::cout << "skill> cleared: " << activeSkillName << "\n\n";
                    logger.log("skill_cleared", {{"name", activeSkillName}});
                    activeSkillName.clear();
                    activeSkillTarget.clear();
                    activeAllowedTools.clear();
                }
                continue;
            }

            // ── /load [path] — resume a previous session ─────────────
            const std::string loadPrefix = "/load";
            if (commandInput == loadPrefix || commandInput.rfind("/load ", 0) == 0) {
                std::filesystem::path loadPath;

                // /load without arguments → show file selector.
                if (commandInput == loadPrefix) {
                    const auto files = listHistoryFiles(std::filesystem::path(appConfig.historyDir));
                    if (files.empty()) {
                        std::cout << "load> no logs found in " << appConfig.historyDir << "\n\n";
                        continue;
                    }

                    std::cout << "load> select a session to resume:\n\n";
                    for (std::size_t i = 0; i < files.size(); ++i) {
                        std::cout << "  [" << (i + 1) << "] "
                                  << files[i].modifiedTime << "  "
                                  << std::setw(7) << files[i].size << " bytes  ";
                        if (!files[i].title.empty()) {
                            std::cout << files[i].title;
                        } else {
                            std::cout << "(no title)";
                        }
                        std::cout << "\n";
                    }
                    std::cout << "  [0] cancel\n\n";

                    std::cout << "\033[36m▸ load\033[0m ";
                    std::string selection;
                    if (!std::getline(std::cin, selection)) {
                        std::cout << "\n";
                        continue;
                    }
                    selection = trimCommandInput(selection);

                    if (selection == "0" || selection.empty()) {
                        std::cout << "load> cancelled.\n\n";
                        continue;
                    }

                    // Try numeric index.
                    try {
                        const int index = std::stoi(selection);
                        if (index >= 1 && static_cast<std::size_t>(index) <= files.size()) {
                            loadPath = files[static_cast<std::size_t>(index) - 1].path;
                        }
                    } catch (const std::exception&) {
                        // Not a number — treat as a path.
                    }

                    if (loadPath.empty()) {
                        // Treat input as a file path.
                        loadPath = std::filesystem::path(selection);
                    }
                } else {
                    // /load <path>
                    auto pathStr = trimCommandInput(commandInput.substr(loadPrefix.size()));
                    loadPath = std::filesystem::path(pathStr);
                }

                // Guard: don't load the currently active log file.
                // Use canonical paths when both exist; skip the check
                // gracefully when either path is not a regular file.
                bool sameFile = false;
                std::error_code ec;
                if (std::filesystem::exists(loadPath, ec) &&
                    std::filesystem::exists(logger.path(), ec)) {
                    try {
                        sameFile = std::filesystem::equivalent(loadPath, logger.path(), ec);
                    } catch (const std::filesystem::filesystem_error&) {
                        sameFile = false;
                    }
                }
                if (sameFile) {
                    std::cout << "load> cannot load the currently active log file.\n\n";
                    continue;
                }

                // Parse the history file.
                int skipped = 0;
                std::vector<cpp_ai_agent::core::Message> loadedMessages;
                try {
                    loadedMessages = loadMessagesFromHistory(loadPath, &skipped);
                } catch (const std::exception& ex) {
                    console.printError(std::string("load: ") + ex.what());
                    continue;
                }

                if (loadedMessages.empty()) {
                    std::cout << "load> no messages found in " << loadPath.string() << "\n\n";
                    continue;
                }

                // Rebuild session: fresh System Prompt + loaded messages.
                session = Session("default");
                session.addMessage(makeMessage(Role::System, systemPrompt));

                // Print a brief summary to help the user recall the context.
                std::cout << "loaded> " << loadPath.filename().string()
                          << " (" << loadedMessages.size() << " messages)";
                if (skipped > 0) {
                    std::cout << ", skipped " << skipped << " corrupted lines";
                }
                std::cout << "\n";

                // Show the first user message as a memory prompt.
                for (const auto& msg : loadedMessages) {
                    if (msg.role == Role::User) {
                        const auto preview = msg.content.size() > 200
                                                 ? msg.content.substr(0, 200) + "..."
                                                 : msg.content;
                        std::cout << "context> " << preview << "\n\n";
                        break;
                    }
                }

                for (const auto& msg : loadedMessages) {
                    session.addMessage(msg);
                }

                firstUserMessage = false;
                logger.log("session_loaded", {
                    {"source", loadPath.string()},
                    {"messages", loadedMessages.size()},
                });
                continue;
            }

            // Rename the log file after the first meaningful user message.
            if (firstUserMessage) {
                firstUserMessage = false;
                // Use the first line or first 60 characters as the title.
                std::string title = commandInput;
                const auto newlinePos = title.find('\n');
                if (newlinePos != std::string::npos) {
                    title = title.substr(0, newlinePos);
                }
                if (title.size() > 60) {
                    title = title.substr(0, 60);
                }
                logger.rename(title);
                std::cout << "log> " << logger.path().filename().string() << "\n";
            }

            if (!commandInput.empty() && commandInput.front() == '/') {
                console.printWarning("Unknown slash command: " + commandInput);
                std::cout << "hint> Type / to show available commands.\n\n";
                continue;
            }

            session.addMessage(makeMessage(Role::User, input));
            logger.log("user_message", {{"content", input}});

            try {
                agentLoop.runTurn(session);
            } catch (const std::exception& ex) {
                console.printError(ex.what());
                std::cout << "hint> Check OPENAI_API_KEY, config/settings.json, and network access.\n";
            }
        }
    } catch (const std::exception& ex) {
        console.printError(std::string("startup: ") + ex.what());
        std::cout << "hint> Set OPENAI_API_KEY before starting the program.\n";
        std::cout << "      PowerShell example: $env:OPENAI_API_KEY='your_key'\n";
        return 1;
    }

    std::cout << "bye\n";
    return 0;
}
