#include "tools/FileTools.h"

#include "tools/PathUtils.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cpp_ai_agent::tools {

namespace {

std::string readWholeFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open file for reading: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void writeWholeFile(const std::filesystem::path& path, const std::string& content) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Failed to open file for writing: " + path.string());
    }

    output << content;
}

nlohmann::json stringProperty(const std::string& description) {
    return {
        {"type", "string"},
        {"description", description},
    };
}

ToolResult failureResult(const std::exception& ex) {
    return {
        false,
        ex.what(),
        std::string("Tool failed: ") + ex.what(),
    };
}

std::string truncatePreview(const std::string& value, std::size_t maxLength = 4000) {
    if (value.size() <= maxLength) {
        return value;
    }
    return value.substr(0, maxLength) + "\n... diff preview truncated ...";
}

std::string linePrefix(const std::string& prefix, const std::string& text) {
    std::ostringstream output;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        output << prefix << text.substr(start, end == std::string::npos ? std::string::npos : end - start) << "\n";
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return output.str();
}

std::string makeReplacementDiff(
    const std::filesystem::path& path,
    const std::string& oldText,
    const std::string& newText
) {
    std::ostringstream output;
    output << "diff preview\n";
    output << "--- " << path.string() << "\n";
    output << "+++ " << path.string() << "\n";
    output << "@@ replacement @@\n";
    output << linePrefix("-", oldText);
    output << linePrefix("+", newText);
    return truncatePreview(output.str());
}

std::string makeWholeFileDiff(
    const std::filesystem::path& path,
    const std::string& oldContent,
    const std::string& newContent
) {
    std::ostringstream output;
    output << "diff preview\n";
    output << "--- " << path.string() << "\n";
    output << "+++ " << path.string() << "\n";
    output << "@@ full file @@\n";
    if (oldContent.empty()) {
        output << "(new file)\n";
    } else {
        output << linePrefix("-", oldContent);
    }
    output << linePrefix("+", newContent);
    return truncatePreview(output.str());
}

}  // namespace

ReadFileTool::ReadFileTool(std::filesystem::path workspaceRoot)
    : workspaceRoot_(std::move(workspaceRoot)) {}

std::string ReadFileTool::name() const {
    return "read_file";
}

std::string ReadFileTool::description() const {
    return "Read a UTF-8 or text file inside the workspace.";
}

nlohmann::json ReadFileTool::parametersSchema() const {
    return {
        {"type", "object"},
        {"properties", {{"path", stringProperty("Path to read, relative to the workspace.")}}},
        {"required", nlohmann::json::array({"path"})},
        {"additionalProperties", false},
    };
}

RiskLevel ReadFileTool::risk() const {
    return RiskLevel::Safe;
}

ToolResult ReadFileTool::execute(const nlohmann::json& args) const {
    try {
        const auto path = resolveWorkspacePath(workspaceRoot_, args.at("path").get<std::string>());
        const auto content = readWholeFile(path);
        return {
            true,
            content,
            "Read file: " + path.string(),
        };
    } catch (const std::exception& ex) {
        return failureResult(ex);
    }
}

WriteFileTool::WriteFileTool(std::filesystem::path workspaceRoot)
    : workspaceRoot_(std::move(workspaceRoot)) {}

std::string WriteFileTool::name() const {
    return "write_file";
}

std::string WriteFileTool::description() const {
    return "Write content to a file inside the workspace, creating parent directories if needed.";
}

nlohmann::json WriteFileTool::parametersSchema() const {
    return {
        {"type", "object"},
        {"properties",
         {
             {"path", stringProperty("Path to write, relative to the workspace.")},
             {"content", stringProperty("Full file content to write.")},
         }},
        {"required", nlohmann::json::array({"path", "content"})},
        {"additionalProperties", false},
    };
}

RiskLevel WriteFileTool::risk() const {
    return RiskLevel::Write;
}

std::string WriteFileTool::preview(const nlohmann::json& args) const {
    try {
        const auto path = resolveWorkspacePath(workspaceRoot_, args.at("path").get<std::string>());
        const auto newContent = args.at("content").get<std::string>();
        const auto oldContent = std::filesystem::exists(path) ? readWholeFile(path) : "";
        return makeWholeFileDiff(path, oldContent, newContent);
    } catch (const std::exception& ex) {
        return std::string("diff preview unavailable: ") + ex.what();
    }
}

ToolResult WriteFileTool::execute(const nlohmann::json& args) const {
    try {
        const auto path = resolveWorkspacePath(workspaceRoot_, args.at("path").get<std::string>());
        writeWholeFile(path, args.at("content").get<std::string>());
        return {
            true,
            "Wrote file: " + path.string(),
            "Wrote file: " + path.string(),
        };
    } catch (const std::exception& ex) {
        return failureResult(ex);
    }
}

EditFileTool::EditFileTool(std::filesystem::path workspaceRoot)
    : workspaceRoot_(std::move(workspaceRoot)) {}

std::string EditFileTool::name() const {
    return "edit_file";
}

std::string EditFileTool::description() const {
    return "Edit a file inside the workspace by replacing one exact text fragment.";
}

nlohmann::json EditFileTool::parametersSchema() const {
    return {
        {"type", "object"},
        {"properties",
         {
             {"path", stringProperty("Path to edit, relative to the workspace.")},
             {"old_text", stringProperty("Exact text to replace.")},
             {"new_text", stringProperty("Replacement text.")},
         }},
        {"required", nlohmann::json::array({"path", "old_text", "new_text"})},
        {"additionalProperties", false},
    };
}

RiskLevel EditFileTool::risk() const {
    return RiskLevel::Write;
}

std::string EditFileTool::preview(const nlohmann::json& args) const {
    try {
        const auto path = resolveWorkspacePath(workspaceRoot_, args.at("path").get<std::string>());
        const auto content = readWholeFile(path);
        const auto oldText = args.at("old_text").get<std::string>();
        const auto newText = args.at("new_text").get<std::string>();
        if (oldText.empty()) {
            throw std::invalid_argument("old_text must not be empty.");
        }
        if (content.find(oldText) == std::string::npos) {
            throw std::runtime_error("old_text was not found in file: " + path.string());
        }
        return makeReplacementDiff(path, oldText, newText);
    } catch (const std::exception& ex) {
        return std::string("diff preview unavailable: ") + ex.what();
    }
}

ToolResult EditFileTool::execute(const nlohmann::json& args) const {
    try {
        const auto path = resolveWorkspacePath(workspaceRoot_, args.at("path").get<std::string>());
        auto content = readWholeFile(path);
        const auto oldText = args.at("old_text").get<std::string>();
        const auto position = content.find(oldText);

        if (oldText.empty()) {
            throw std::invalid_argument("old_text must not be empty.");
        }
        if (position == std::string::npos) {
            throw std::runtime_error("old_text was not found in file: " + path.string());
        }

        content.replace(position, oldText.size(), args.at("new_text").get<std::string>());
        writeWholeFile(path, content);

        return {
            true,
            "Edited file: " + path.string(),
            "Edited file: " + path.string(),
        };
    } catch (const std::exception& ex) {
        return failureResult(ex);
    }
}

}  // namespace cpp_ai_agent::tools
