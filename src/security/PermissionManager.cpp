#include "security/PermissionManager.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace cpp_ai_agent::security {

namespace {

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

}  // namespace

PermissionManager::PermissionManager(PermissionMode mode, PermissionPrompt prompt)
    : mode_(mode), prompt_(std::move(prompt)) {}

bool PermissionManager::approve(const PermissionRequest& request) const {
    if (request.risk == tools::RiskLevel::Safe) {
        return true;
    }

    PermissionMode mode = PermissionMode::AskEachTime;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mode = mode_;
    }

    if (mode == PermissionMode::ReadOnly) {
        return false;
    }

    if (mode == PermissionMode::TrustSession) {
        if (request.toolName == "run_command" && isBlockedDangerousCommand(request.arguments)) {
            return prompt_ ? prompt_(request) : false;
        }
        return true;
    }

    return prompt_ ? prompt_(request) : false;
}

PermissionMode PermissionManager::mode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mode_;
}

void PermissionManager::setMode(PermissionMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    mode_ = mode;
}

PermissionMode permissionModeFromString(const std::string& value) {
    const auto normalized = lowerCopy(value);
    if (normalized == "read_only") {
        return PermissionMode::ReadOnly;
    }
    if (normalized == "trust_session") {
        return PermissionMode::TrustSession;
    }
    return PermissionMode::AskEachTime;
}

std::string riskLevelToString(tools::RiskLevel risk) {
    switch (risk) {
        case tools::RiskLevel::Safe:
            return "safe";
        case tools::RiskLevel::Write:
            return "write";
        case tools::RiskLevel::Dangerous:
            return "dangerous";
    }

    return "unknown";
}

bool isBlockedDangerousCommand(const std::string& command) {
    const auto normalized = lowerCopy(command);
    const char* blocked[] = {
        "format ",
        "del /s",
        "rmdir /s",
        "rd /s",
        "remove-item",
        "rm -rf",
        "git reset --hard",
        "shutdown",
    };

    for (const auto* pattern : blocked) {
        if (normalized.find(pattern) != std::string::npos) {
            return true;
        }
    }

    return false;
}

}  // namespace cpp_ai_agent::security
