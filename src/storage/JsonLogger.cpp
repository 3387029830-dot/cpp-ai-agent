#include "storage/JsonLogger.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace cpp_ai_agent::storage {

namespace {

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
    stream << std::put_time(&localTime, "%Y%m%d-%H%M%S");
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

}  // namespace cpp_ai_agent::storage
