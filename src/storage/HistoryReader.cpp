#include "storage/HistoryReader.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace cpp_ai_agent::storage {

namespace {

std::string formatFileTime(const std::filesystem::file_time_type& fileTime) {
    const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
    );
    const auto time = std::chrono::system_clock::to_time_t(systemTime);

    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

std::string compact(const std::string& value, std::size_t maxLength = 600) {
    if (value.size() <= maxLength) {
        return value;
    }

    return value.substr(0, maxLength) + "...";
}

std::string formatEntry(const nlohmann::json& entry) {
    const auto type = entry.value("type", "unknown");
    const auto data = entry.value("data", nlohmann::json::object());

    if (type == "user_message") {
        return "user> " + data.value("content", "");
    }
    if (type == "assistant_message") {
        return "assistant> " + data.value("content", "");
    }
    if (type == "tool_call") {
        return "tool_call> " + data.value("name", "") + " " + data.value("arguments", "");
    }
    if (type == "tool_result") {
        return "tool_result> " + compact(data.value("content", ""));
    }
    if (type == "warning") {
        return "warning> " + data.value("content", "");
    }

    return type + "> " + data.dump();
}

/// Extract the session title from a log filename.
///
/// Expected format:  session-YYYYMMDD-HHMMSS[-title].jsonl
/// Returns the title portion, or an empty string for old-format names
/// that have only the timestamp.
std::string parseTitleFromFilename(const std::string& filename) {
    // Strip ".jsonl" extension.
    std::string stem = filename;
    const auto dotPos = stem.rfind(".jsonl");
    if (dotPos != std::string::npos) {
        stem = stem.substr(0, dotPos);
    }

    // The stem should start with "session-".
    const std::string prefix = "session-";
    if (stem.rfind(prefix, 0) != 0) {
        return "";
    }

    // After "session-YYYYMMDD-HHMMSS" there may be "-title".
    // The timestamp part is exactly 15 characters (8 digits + 1 hyphen + 6 digits).
    const std::size_t tsLen = prefix.size() + 15;  // "session-" + "YYYYMMDD-HHMMSS"
    if (stem.size() <= tsLen) {
        return "";  // no title portion
    }

    // The character after the timestamp must be '-'.
    if (stem[tsLen] != '-') {
        return "";
    }

    return stem.substr(tsLen + 1);
}

}  // namespace

std::vector<HistoryFile> listHistoryFiles(const std::filesystem::path& historyDir) {
    std::vector<HistoryFile> files;
    if (!std::filesystem::exists(historyDir)) {
        return files;
    }

    for (const auto& entry : std::filesystem::directory_iterator(historyDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".jsonl") {
            continue;
        }

        files.push_back({
            entry.path(),
            entry.file_size(),
            formatFileTime(entry.last_write_time()),
            parseTitleFromFilename(entry.path().filename().string()),
        });
    }

    std::sort(files.begin(), files.end(), [](const HistoryFile& lhs, const HistoryFile& rhs) {
        return lhs.path.filename().string() > rhs.path.filename().string();
    });
    return files;
}

std::vector<std::string> replayHistoryFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open history file: " + path.string());
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        try {
            lines.push_back(formatEntry(nlohmann::json::parse(line)));
        } catch (const std::exception& ex) {
            lines.push_back(std::string("invalid_log_entry> ") + ex.what());
        }
    }

    return lines;
}

std::vector<core::Message> loadMessagesFromHistory(
    const std::filesystem::path& path,
    int* skippedLines) {

    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open history file: " + path.string());
    }

    std::vector<core::Message> messages;
    core::Message pendingAssistant;
    bool hasPendingAssistant = false;
    std::vector<core::Message> pendingToolMessages;
    int skipped = 0;

    // Flush the current assistant (with all its tool_calls) followed by
    // any buffered tool-result messages.  This must be called BEFORE
    // starting a new assistant or user message so that the API-required
    // ordering is preserved:
    //   Assistant (with tool_calls) → Tool → Tool → ...
    auto flushPending = [&]() {
        if (hasPendingAssistant) {
            messages.push_back(std::move(pendingAssistant));
            pendingAssistant = {};
            hasPendingAssistant = false;
        }
        for (auto& tm : pendingToolMessages) {
            messages.push_back(std::move(tm));
        }
        pendingToolMessages.clear();
    };

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        nlohmann::json entry;
        try {
            entry = nlohmann::json::parse(line);
        } catch (const nlohmann::json::parse_error&) {
            ++skipped;
            continue;
        }

        const auto type = entry.value("type", "");
        const auto data = entry.value("data", nlohmann::json::object());
        const auto timestamp = entry.value("timestamp", std::int64_t{0});

        if (type == "user_message") {
            flushPending();
            core::Message msg;
            msg.role = core::Role::User;
            msg.content = data.value("content", "");
            msg.timestamp = timestamp;
            messages.push_back(std::move(msg));
        } else if (type == "assistant_message") {
            flushPending();
            pendingAssistant.role = core::Role::Assistant;
            pendingAssistant.content = data.value("content", "");
            pendingAssistant.timestamp = timestamp;
            hasPendingAssistant = true;
        } else if (type == "tool_call") {
            if (hasPendingAssistant) {
                core::ToolCall tc;
                tc.id = data.value("id", "");
                tc.name = data.value("name", "");
                tc.arguments = data.value("arguments", "");
                pendingAssistant.toolCalls.push_back(std::move(tc));
            }
            // If there's no pending assistant, this tool_call is orphaned — skip it.
        } else if (type == "tool_result") {
            // Buffer the tool result; it will be flushed together with its
            // parent assistant when the next assistant or user message arrives.
            // This preserves multi-iteration agent loops where several
            // assistant→tools blocks appear between two user messages.
            core::Message msg;
            msg.role = core::Role::Tool;
            msg.content = data.value("content", "");
            msg.toolCallId = data.value("tool_call_id", "");
            msg.timestamp = timestamp;
            pendingToolMessages.push_back(std::move(msg));
        }
        // skill_selected, skill_cleared, warning, and any other meta-events
        // are intentionally skipped so the caller provides a fresh System Prompt.
    }

    flushPending();

    if (skippedLines != nullptr) {
        *skippedLines = skipped;
    }

    return messages;
}

}  // namespace cpp_ai_agent::storage
