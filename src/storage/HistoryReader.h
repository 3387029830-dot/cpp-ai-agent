#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace cpp_ai_agent::storage {

struct HistoryFile {
    std::filesystem::path path;
    std::uintmax_t size = 0;
    std::string modifiedTime;
};

std::vector<HistoryFile> listHistoryFiles(const std::filesystem::path& historyDir);
std::vector<std::string> replayHistoryFile(const std::filesystem::path& path);

}  // namespace cpp_ai_agent::storage
