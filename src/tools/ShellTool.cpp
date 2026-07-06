#include "tools/ShellTool.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace cpp_ai_agent::tools {

namespace {

ToolResult failureResult(const std::exception& ex) {
    return {
        false,
        ex.what(),
        std::string("Tool failed: ") + ex.what(),
    };
}

}  // namespace

std::string ShellTool::name() const {
    return "run_command";
}

std::string ShellTool::description() const {
    return "Run a shell command and return combined stdout and stderr.";
}

nlohmann::json ShellTool::parametersSchema() const {
    return {
        {"type", "object"},
        {"properties",
         {
             {"command",
              {
                  {"type", "string"},
                  {"description", "Command to execute."},
              }},
         }},
        {"required", nlohmann::json::array({"command"})},
        {"additionalProperties", false},
    };
}

RiskLevel ShellTool::risk() const {
    return RiskLevel::Dangerous;
}

ToolResult ShellTool::execute(const nlohmann::json& args) const {
    try {
        const auto command = args.at("command").get<std::string>();
        if (command.empty()) {
            throw std::invalid_argument("command must not be empty.");
        }

#ifdef _WIN32
        const auto fullCommand = command + " 2>&1";
        FILE* pipe = _popen(fullCommand.c_str(), "r");
#else
        const auto fullCommand = command + " 2>&1";
        FILE* pipe = popen(fullCommand.c_str(), "r");
#endif
        if (pipe == nullptr) {
            throw std::runtime_error("Failed to start command.");
        }

        std::string output;
        std::array<char, 4096> buffer{};
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
            output += buffer.data();
        }

#ifdef _WIN32
        const auto exitCode = _pclose(pipe);
#else
        const auto exitCode = pclose(pipe);
#endif
        const auto success = exitCode == 0;
        return {
            success,
            output,
            "Command exited with code " + std::to_string(exitCode),
        };
    } catch (const std::exception& ex) {
        return failureResult(ex);
    }
}

}  // namespace cpp_ai_agent::tools
