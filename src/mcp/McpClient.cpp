#include "mcp/McpClient.h"

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace cpp_ai_agent::mcp {

namespace {

constexpr const char* protocolVersion = "2025-03-26";

std::runtime_error mcpError(const std::string& message) {
    return std::runtime_error("MCP stdio client: " + message);
}

std::string quoteArg(const std::string& arg) {
    if (arg.empty()) {
        return "\"\"";
    }

    const auto needsQuotes = arg.find_first_of(" \t\"") != std::string::npos;
    if (!needsQuotes) {
        return arg;
    }

    std::string result = "\"";
    std::size_t backslashes = 0;
    for (const char ch : arg) {
        if (ch == '\\') {
            ++backslashes;
        } else if (ch == '"') {
            result.append(backslashes * 2 + 1, '\\');
            result.push_back('"');
            backslashes = 0;
        } else {
            result.append(backslashes, '\\');
            backslashes = 0;
            result.push_back(ch);
        }
    }
    result.append(backslashes * 2, '\\');
    result.push_back('"');
    return result;
}

std::string makeCommandLine(const std::vector<std::string>& command) {
    if (command.empty()) {
        throw mcpError("empty command");
    }

    std::string line;
    for (const auto& arg : command) {
        if (!line.empty()) {
            line.push_back(' ');
        }
        line += quoteArg(arg);
    }
    return line;
}

void ensureJsonRpcResponse(const nlohmann::json& response, int id) {
    if (response.value("jsonrpc", "") != "2.0") {
        throw mcpError("response is not JSON-RPC 2.0");
    }
    if (!response.contains("id") || response.at("id").get<int>() != id) {
        throw mcpError("response id does not match request id");
    }
    if (response.contains("error")) {
        throw mcpError("server returned error: " + response.at("error").dump());
    }
}

#ifdef _WIN32

class ChildProcess {
public:
    explicit ChildProcess(const std::vector<std::string>& command) {
        SECURITY_ATTRIBUTES securityAttributes{};
        securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
        securityAttributes.bInheritHandle = TRUE;

        HANDLE childStdInRead = nullptr;
        HANDLE childStdOutWrite = nullptr;
        if (!CreatePipe(&childStdInRead, &stdinWrite_, &securityAttributes, 0)) {
            throw mcpError("failed to create stdin pipe");
        }
        if (!SetHandleInformation(stdinWrite_, HANDLE_FLAG_INHERIT, 0)) {
            throw mcpError("failed to mark stdin write handle non-inheritable");
        }
        if (!CreatePipe(&stdoutRead_, &childStdOutWrite, &securityAttributes, 0)) {
            throw mcpError("failed to create stdout pipe");
        }
        if (!SetHandleInformation(stdoutRead_, HANDLE_FLAG_INHERIT, 0)) {
            throw mcpError("failed to mark stdout read handle non-inheritable");
        }

        STARTUPINFOA startupInfo{};
        startupInfo.cb = sizeof(STARTUPINFOA);
        startupInfo.dwFlags = STARTF_USESTDHANDLES;
        startupInfo.hStdInput = childStdInRead;
        startupInfo.hStdOutput = childStdOutWrite;
        startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        auto commandLine = makeCommandLine(command);
        PROCESS_INFORMATION processInfo{};
        if (!CreateProcessA(
                nullptr,
                commandLine.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_NO_WINDOW,
                nullptr,
                nullptr,
                &startupInfo,
                &processInfo
            )) {
            CloseHandle(childStdInRead);
            CloseHandle(childStdOutWrite);
            throw mcpError("failed to start server process: " + commandLine);
        }

        CloseHandle(childStdInRead);
        CloseHandle(childStdOutWrite);
        process_ = processInfo.hProcess;
        thread_ = processInfo.hThread;
    }

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

    ~ChildProcess() {
        if (stdinWrite_ != nullptr) {
            CloseHandle(stdinWrite_);
            stdinWrite_ = nullptr;
        }
        if (process_ != nullptr) {
            const auto waitResult = WaitForSingleObject(process_, 1000);
            if (waitResult == WAIT_TIMEOUT) {
                TerminateProcess(process_, 0);
            }
        }
        if (stdoutRead_ != nullptr) {
            CloseHandle(stdoutRead_);
        }
        if (thread_ != nullptr) {
            CloseHandle(thread_);
        }
        if (process_ != nullptr) {
            CloseHandle(process_);
        }
    }

    void writeLine(const std::string& line) const {
        const auto payload = line + "\n";
        DWORD written = 0;
        if (!WriteFile(stdinWrite_, payload.data(), static_cast<DWORD>(payload.size()), &written, nullptr)) {
            throw mcpError("failed to write to server stdin");
        }
        if (written != payload.size()) {
            throw mcpError("short write to server stdin");
        }
    }

    std::string readLine(int timeoutMs = 10000) const {
        std::string line;
        const auto start = std::chrono::steady_clock::now();
        while (true) {
            DWORD available = 0;
            if (!PeekNamedPipe(stdoutRead_, nullptr, 0, nullptr, &available, nullptr)) {
                throw mcpError("failed to read from server stdout");
            }

            if (available == 0) {
                DWORD exitCode = 0;
                if (GetExitCodeProcess(process_, &exitCode) && exitCode != STILL_ACTIVE) {
                    throw mcpError("server process exited before sending a response");
                }
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start
                );
                if (elapsed.count() > timeoutMs) {
                    throw mcpError("timed out waiting for server response");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            char ch = '\0';
            DWORD read = 0;
            if (!ReadFile(stdoutRead_, &ch, 1, &read, nullptr) || read == 0) {
                throw mcpError("failed to read byte from server stdout");
            }
            if (ch == '\n') {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                return line;
            }
            line.push_back(ch);
        }
    }

private:
    HANDLE process_ = nullptr;
    HANDLE thread_ = nullptr;
    HANDLE stdinWrite_ = nullptr;
    HANDLE stdoutRead_ = nullptr;
};

#endif

}  // namespace

nlohmann::json makeInitializeRequest(int id) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", "initialize"},
        {"params",
         {
             {"protocolVersion", protocolVersion},
             {"capabilities", nlohmann::json::object()},
             {"clientInfo", {{"name", "cpp-ai-agent"}, {"version", "0.1.0"}}},
         }},
    };
}

nlohmann::json makeInitializedNotification() {
    return {
        {"jsonrpc", "2.0"},
        {"method", "notifications/initialized"},
        {"params", nlohmann::json::object()},
    };
}

nlohmann::json makeToolsListRequest(int id) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", "tools/list"},
        {"params", nlohmann::json::object()},
    };
}

nlohmann::json makeToolsCallRequest(int id, const std::string& name, const nlohmann::json& arguments) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", "tools/call"},
        {"params", {{"name", name}, {"arguments", arguments}}},
    };
}

McpServerInfo parseInitializeResult(const nlohmann::json& response) {
    ensureJsonRpcResponse(response, response.at("id").get<int>());
    const auto result = response.at("result");
    const auto serverInfo = result.value("serverInfo", nlohmann::json::object());

    McpServerInfo info;
    info.protocolVersion = result.value("protocolVersion", "");
    info.name = serverInfo.value("name", "");
    info.version = serverInfo.value("version", "");
    info.instructions = result.value("instructions", "");
    return info;
}

std::vector<McpTool> parseToolsListResult(const nlohmann::json& response) {
    ensureJsonRpcResponse(response, response.at("id").get<int>());
    std::vector<McpTool> tools;
    const auto result = response.at("result");
    for (const auto& rawTool : result.value("tools", nlohmann::json::array())) {
        McpTool tool;
        tool.name = rawTool.value("name", "");
        tool.description = rawTool.value("description", "");
        tool.inputSchema = rawTool.value("inputSchema", nlohmann::json::object());
        if (!tool.name.empty()) {
            tools.push_back(std::move(tool));
        }
    }
    return tools;
}

McpToolResult parseToolsCallResult(const nlohmann::json& response) {
    ensureJsonRpcResponse(response, response.at("id").get<int>());
    const auto result = response.at("result");

    McpToolResult toolResult;
    toolResult.isError = result.value("isError", false);
    toolResult.rawContent = result.value("content", nlohmann::json::array());

    for (const auto& item : toolResult.rawContent) {
        if (item.value("type", "") != "text") {
            continue;
        }
        if (!toolResult.text.empty()) {
            toolResult.text += "\n";
        }
        toolResult.text += item.value("text", "");
    }
    return toolResult;
}

StdioMcpClient::StdioMcpClient(std::vector<std::string> command) : command_(std::move(command)) {}

McpServerInfo StdioMcpClient::discoverTools() const {
#ifdef _WIN32
    ChildProcess process(command_);
    constexpr int initializeId = 1;
    constexpr int toolsListId = 2;

    process.writeLine(makeInitializeRequest(initializeId).dump());
    auto info = parseInitializeResult(nlohmann::json::parse(process.readLine()));

    process.writeLine(makeInitializedNotification().dump());
    process.writeLine(makeToolsListRequest(toolsListId).dump());
    info.tools = parseToolsListResult(nlohmann::json::parse(process.readLine()));
    return info;
#else
    throw mcpError("stdio process transport is currently implemented for Windows only");
#endif
}

McpToolResult StdioMcpClient::callTool(const std::string& name, const nlohmann::json& arguments) const {
#ifdef _WIN32
    ChildProcess process(command_);
    constexpr int initializeId = 1;
    constexpr int toolCallId = 2;

    process.writeLine(makeInitializeRequest(initializeId).dump());
    parseInitializeResult(nlohmann::json::parse(process.readLine()));

    process.writeLine(makeInitializedNotification().dump());
    process.writeLine(makeToolsCallRequest(toolCallId, name, arguments).dump());
    return parseToolsCallResult(nlohmann::json::parse(process.readLine()));
#else
    throw mcpError("stdio process transport is currently implemented for Windows only");
#endif
}

}  // namespace cpp_ai_agent::mcp
