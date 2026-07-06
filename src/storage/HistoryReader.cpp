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

}  // namespace cpp_ai_agent::storage
