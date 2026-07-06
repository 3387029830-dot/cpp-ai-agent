#pragma once

#include <filesystem>
#include <string>

namespace cpp_ai_agent::tools {

std::filesystem::path resolveWorkspacePath(
    const std::filesystem::path& workspaceRoot,
    const std::string& requestedPath
);

bool isPathInsideWorkspace(
    const std::filesystem::path& workspaceRoot,
    const std::filesystem::path& candidatePath
);

}  // namespace cpp_ai_agent::tools
