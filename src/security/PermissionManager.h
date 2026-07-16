#pragma once

#include "tools/ITool.h"

#include <functional>
#include <mutex>
#include <string>

namespace cpp_ai_agent::security {

enum class PermissionMode {
    AskEachTime,
    ReadOnly,
    TrustSession,
};

struct PermissionRequest {
    std::string toolName;
    tools::RiskLevel risk = tools::RiskLevel::Safe;
    std::string arguments;
    std::string preview;
};

using PermissionPrompt = std::function<bool(const PermissionRequest&)>;

class PermissionManager {
public:
    PermissionManager(PermissionMode mode, PermissionPrompt prompt = {});

    bool approve(const PermissionRequest& request) const;
    PermissionMode mode() const;
    void setMode(PermissionMode mode);

private:
    mutable std::mutex mutex_;
    PermissionMode mode_ = PermissionMode::AskEachTime;
    PermissionPrompt prompt_;
};

PermissionMode permissionModeFromString(const std::string& value);
std::string riskLevelToString(tools::RiskLevel risk);
bool isBlockedDangerousCommand(const std::string& command);

}  // namespace cpp_ai_agent::security
