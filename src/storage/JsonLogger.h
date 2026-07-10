#pragma once

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace cpp_ai_agent::storage {

class JsonLogger {
public:
    explicit JsonLogger(std::filesystem::path historyDir);

    void log(const std::string& type, const nlohmann::json& data);
    const std::filesystem::path& path() const;

    /// Rename the current log file to include a session title.
    ///
    /// The new filename is derived from the current timestamp-based name:
    ///   session-YYYYMMDD-HHMMSS-<cleanTitle>.jsonl
    /// Invalid filesystem characters are removed, whitespace is replaced
    /// with '-', and the title is truncated to 24 characters.
    /// If renaming fails the original filename is kept and a warning is
    /// printed to stderr — the session continues uninterrupted.
    void rename(std::string_view newTitle);

private:
    std::filesystem::path path_;
    std::ofstream output_;
};

}  // namespace cpp_ai_agent::storage
