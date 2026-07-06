#pragma once

#include "config/AppConfig.h"

#include <string>
#include <vector>

namespace cpp_ai_agent::diagnostics {

struct DiagnosticItem {
    std::string status;
    std::string name;
    std::string detail;
};

std::vector<DiagnosticItem> runLocalDiagnostics(const config::AppConfig& config);
std::vector<std::string> formatDiagnostics(const std::vector<DiagnosticItem>& items);
std::vector<std::string> formatConfigSummary(const config::AppConfig& config);

}  // namespace cpp_ai_agent::diagnostics
