#include "tools/PathUtils.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace cpp_ai_agent::tools {

namespace {

std::filesystem::path normalizeExistingPrefix(const std::filesystem::path& path) {
    std::error_code error;
    auto canonical = std::filesystem::weakly_canonical(path, error);
    if (error) {
        return path.lexically_normal();
    }
    return canonical;
}

std::string pathToComparableString(const std::filesystem::path& path) {
    auto value = path.lexically_normal().generic_string();
#ifdef _WIN32
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
#endif
    return value;
}

}  // namespace

std::filesystem::path resolveWorkspacePath(
    const std::filesystem::path& workspaceRoot,
    const std::string& requestedPath
) {
    if (requestedPath.empty()) {
        throw std::invalid_argument("Path argument is required.");
    }

    const auto root = normalizeExistingPrefix(workspaceRoot);
    const auto requested = std::filesystem::path(requestedPath);
    const auto candidate = requested.is_absolute() ? requested : root / requested;
    const auto normalized = normalizeExistingPrefix(candidate);

    if (!isPathInsideWorkspace(root, normalized)) {
        throw std::runtime_error("Path is outside the workspace: " + requestedPath);
    }

    return normalized;
}

bool isPathInsideWorkspace(
    const std::filesystem::path& workspaceRoot,
    const std::filesystem::path& candidatePath
) {
    const auto root = pathToComparableString(normalizeExistingPrefix(workspaceRoot));
    const auto candidate = pathToComparableString(normalizeExistingPrefix(candidatePath));

    if (candidate == root) {
        return true;
    }

    const auto prefix = root.back() == '/' ? root : root + "/";
    return candidate.rfind(prefix, 0) == 0;
}

}  // namespace cpp_ai_agent::tools
