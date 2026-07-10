#include "storage/JsonLogger.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
#include <sstream>
#include <stdexcept>

namespace cpp_ai_agent::storage {

namespace {

int processId() {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

std::string timestampForFileName() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream stream;
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()
                        ).count() % 1000;
    stream << std::put_time(&localTime, "%Y%m%d-%H%M%S")
           << "-" << std::setw(3) << std::setfill('0') << millis
           << "-" << processId();
    return stream.str();
}

std::int64_t timestampMillis() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

}  // namespace

JsonLogger::JsonLogger(std::filesystem::path historyDir) {
    std::filesystem::create_directories(historyDir);
    path_ = historyDir / ("session-" + timestampForFileName() + ".jsonl");
    output_.open(path_, std::ios::out | std::ios::app);
    if (!output_) {
        throw std::runtime_error("Failed to open log file: " + path_.string());
    }
}

void JsonLogger::log(const std::string& type, const nlohmann::json& data) {
    nlohmann::json entry = {
        {"timestamp", timestampMillis()},
        {"type", type},
        {"data", data},
    };
    output_ << entry.dump() << '\n';
    output_.flush();
}

const std::filesystem::path& JsonLogger::path() const {
    return path_;
}

void JsonLogger::rename(std::string_view newTitle) {
    if (newTitle.empty()) {
        return;
    }

    // ── Sanitize the title for use as a filename segment ──────────────
    std::string clean;
    clean.reserve(newTitle.size());

    for (const char ch : newTitle) {
        // Reject characters that are illegal on Windows and/or problematic
        // in filenames: < > : " / \ | ? *
        if (ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
            ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*') {
            continue;
        }
        // Collapse whitespace into a single '-'.
        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            if (clean.empty() || clean.back() != '-') {
                clean.push_back('-');
            }
            continue;
        }
        clean.push_back(ch);
    }

    // Collapse consecutive '-' separators.
    clean.erase(std::unique(clean.begin(), clean.end(),
                            [](char a, char b) { return a == '-' && b == '-'; }),
                clean.end());

    // Trim leading / trailing '-'.
    while (!clean.empty() && clean.front() == '-') {
        clean.erase(clean.begin());
    }
    while (!clean.empty() && clean.back() == '-') {
        clean.pop_back();
    }

    // Truncate to at most 24 characters (code points, which for ASCII
    // and BMP characters is also the byte count).
    if (clean.size() > 24) {
        clean.resize(24);
        // Don't leave a trailing '-' after truncation.
        while (!clean.empty() && clean.back() == '-') {
            clean.pop_back();
        }
    }

    if (clean.empty()) {
        return;
    }

    // ── Build the new path ────────────────────────────────────────────
    const auto parentDir = path_.parent_path();
    const auto stem = path_.stem().string();       // e.g. "session-20260710-143022"
    const auto ext = path_.extension().string();   // e.g. ".jsonl"

    const auto newPath = parentDir / (stem + "-" + clean + ext);
    if (newPath == path_) {
        return;  // already has the same name
    }

    // ── Close → rename → reopen ───────────────────────────────────────
    output_.close();

    try {
        std::filesystem::rename(path_, newPath);
        path_ = newPath;
    } catch (const std::filesystem::filesystem_error& ex) {
        std::cerr << "warning> failed to rename log file: " << ex.what() << "\n";
    }

    output_.open(path_, std::ios::out | std::ios::app);
    if (!output_) {
        std::cerr << "warning> failed to reopen log file after rename: "
                  << path_.string() << "\n";
        // Fall back to a fresh log file so subsequent log() calls
        // are not silently discarded into a dead stream.
        path_ = parentDir / ("session-" + timestampForFileName() + ".jsonl");
        output_.clear();
        output_.open(path_, std::ios::out | std::ios::app);
        if (output_) {
            std::cerr << "warning> logging redirected to " << path_.string() << "\n";
        }
    }
}

}  // namespace cpp_ai_agent::storage
