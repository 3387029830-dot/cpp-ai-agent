#include "utils/Encoding.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace cpp_ai_agent::utils {

bool isValidUtf8(const std::string& value) {
    if (value.empty()) {
        return true;
    }

#ifdef _WIN32
    return MultiByteToWideChar(
               CP_UTF8,
               MB_ERR_INVALID_CHARS,
               value.data(),
               static_cast<int>(value.size()),
               nullptr,
               0
           ) > 0;
#else
    // On non-Windows, assume valid; there's no portable single-function
    // check without pulling in a heavy library.
    return true;
#endif
}

std::string toUtf8(const std::string& value, unsigned int sourceCodePage) {
    if (value.empty()) {
        return value;
    }

#ifdef _WIN32
    // Step 1: sourceCodePage → wide string
    const auto wideLength = MultiByteToWideChar(
        sourceCodePage,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );
    if (wideLength <= 0) {
        return value;  // conversion failed, return original
    }

    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    MultiByteToWideChar(
        sourceCodePage,
        0,
        value.data(),
        static_cast<int>(value.size()),
        wide.data(),
        wideLength
    );

    // Step 2: wide string → UTF-8
    const auto utf8Length = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.data(),
        wideLength,
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (utf8Length <= 0) {
        return value;  // conversion failed, return original
    }

    std::string utf8(static_cast<std::size_t>(utf8Length), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.data(),
        wideLength,
        utf8.data(),
        utf8Length,
        nullptr,
        nullptr
    );
    return utf8;
#else
    return value;
#endif
}

std::string ensureUtf8(const std::string& value) {
    if (isValidUtf8(value)) {
        return value;
    }

#ifdef _WIN32
    // Try system ANSI code page (often GBK on Chinese Windows).
    auto converted = toUtf8(value, CP_ACP);
    if (isValidUtf8(converted)) {
        return converted;
    }

    // Try GBK / CP 936 explicitly (Chinese Simplified).
    converted = toUtf8(value, 936);
    if (isValidUtf8(converted)) {
        return converted;
    }

    // Try Big5 / CP 950 (Chinese Traditional).
    converted = toUtf8(value, 950);
    if (isValidUtf8(converted)) {
        return converted;
    }
#endif

    // All conversions failed; return original to avoid data loss.
    return value;
}

}  // namespace cpp_ai_agent::utils
