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

private:
    std::filesystem::path path_;
    std::ofstream output_;
};

}  // namespace cpp_ai_agent::storage
