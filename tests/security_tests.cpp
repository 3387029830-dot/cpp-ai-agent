#include <doctest/doctest.h>

#include "security/PermissionManager.h"

using cpp_ai_agent::security::PermissionManager;
using cpp_ai_agent::security::PermissionMode;
using cpp_ai_agent::security::PermissionRequest;
using cpp_ai_agent::tools::RiskLevel;

TEST_CASE("PermissionManager always allows safe tools") {
    PermissionManager permissions(PermissionMode::ReadOnly);

    CHECK(permissions.approve({"read_file", RiskLevel::Safe, R"({"path":"README.md"})"}));
}

TEST_CASE("PermissionManager rejects write tools in read-only mode") {
    PermissionManager permissions(PermissionMode::ReadOnly);

    CHECK_FALSE(permissions.approve({"write_file", RiskLevel::Write, R"({"path":"x"})"}));
}

TEST_CASE("PermissionManager asks before write tools in ask-each-time mode") {
    bool prompted = false;
    PermissionManager permissions(PermissionMode::AskEachTime, [&](const PermissionRequest&) {
        prompted = true;
        return true;
    });

    CHECK(permissions.approve({"edit_file", RiskLevel::Write, R"({"path":"x"})"}));
    CHECK(prompted);
}

TEST_CASE("PermissionManager blocks obviously destructive commands") {
    PermissionManager permissions(PermissionMode::TrustSession);

    CHECK_FALSE(permissions.approve({"run_command", RiskLevel::Dangerous, R"({"command":"git reset --hard"})"}));
}
