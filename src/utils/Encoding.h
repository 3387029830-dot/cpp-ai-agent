#pragma once

#include <string>

namespace cpp_ai_agent::utils {

/// Returns true if `value` is valid UTF-8 (or empty).
bool isValidUtf8(const std::string& value);

/// Convert `value` from `sourceCodePage` to UTF-8.
/// Returns the converted string, or the original if conversion fails.
std::string toUtf8(const std::string& value, unsigned int sourceCodePage);

/// Ensure `value` is valid UTF-8 by trying common encodings:
///   1. Already valid UTF-8 → return as-is.
///   2. Try the system ANSI code page (CP_ACP).
///   3. Try GBK / CP 936.
///   4. Try Big5 / CP 950.
///   5. Final fallback: round-trip through WideChar to replace invalid
///      sequences with U+FFFD (never returns non-UTF-8).
std::string ensureUtf8(const std::string& value);

/// Replace invalid UTF-8 byte sequences with U+FFFD (replacement character).
/// Uses the OS wide-char round-trip to sanitize; always returns valid UTF-8.
std::string sanitizeUtf8(const std::string& value);

}  // namespace cpp_ai_agent::utils
