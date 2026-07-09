#include "ui/Console.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace cpp_ai_agent::ui {

namespace {

// ANSI SGR (Select Graphic Rendition) escape sequences.
// Each constant is a CSI (Control Sequence Introducer, \033[) followed by
// the SGR parameter(s) and the final byte 'm'.
// Reference: ECMA-48 / ISO 6429.
constexpr const char* reset   = "\033[0m";   // reset all attributes
constexpr const char* bold    = "\033[1m";   // bold / increased intensity
constexpr const char* cyan    = "\033[36m";  // foreground cyan
constexpr const char* blue    = "\033[34m";  // foreground blue
constexpr const char* gray    = "\033[90m";  // bright black (gray)
constexpr const char* yellow  = "\033[33m";  // foreground yellow
constexpr const char* red     = "\033[31m";  // foreground red
constexpr const char* inverse = "\033[7m";   // swap foreground & background

// Returns the number of bytes in a UTF-8 code point given its first byte.
// See RFC 3629, Table 3-6: UTF-8 Bit Distribution.
std::size_t utf8CharLen(unsigned char firstByte) {
    // 0xxxxxxx — single ASCII byte (U+0000..U+007F).
    if (firstByte < 0x80) {
        return 1;
    }
    // 110xxxxx — 2-byte sequence (U+0080..U+07FF).
    // (firstByte >> 5) == 0b110  →  0xC0..0xDF.
    if ((firstByte >> 5) == 0x06) {
        return 2;
    }
    // 1110xxxx — 3-byte sequence (U+0800..U+FFFF).
    // (firstByte >> 4) == 0b1110  →  0xE0..0xEF.
    if ((firstByte >> 4) == 0x0E) {
        return 3;
    }
    // 11110xxx — 4-byte sequence (U+10000..U+10FFFF).
    // (firstByte >> 3) == 0b11110  →  0xF0..0xF7.
    if ((firstByte >> 3) == 0x1E) {
        return 4;
    }
    // Continuation bytes (10xxxxxx) or invalid lead bytes — treat as 1 byte.
    return 1;
}

// Strips the "https://" or "http://" scheme prefix from a URL so that it
// reads better in compact status lines (e.g. "api.deepseek.com").
std::string stripScheme(std::string value) {
    const std::string https = "https://";
    const std::string http = "http://";
    if (value.rfind(https, 0) == 0) {
        return value.substr(https.size());
    }
    if (value.rfind(http, 0) == 0) {
        return value.substr(http.size());
    }
    return value;
}

// Reads an environment variable, returning "" when unset or empty.
// On Windows this uses _dupenv_s (the CRT secure variant) with a
// unique_ptr+free guard so the allocated buffer is always released.
std::string readEnv(const char* name) {
#ifdef _WIN32
    char* raw = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&raw, &size, name) != 0 || raw == nullptr) {
        return "";
    }
    std::unique_ptr<char, decltype(&std::free)> value(raw, &std::free);
    return std::string(value.get());
#else
    const char* value = std::getenv(name);
    return value == nullptr ? "" : std::string(value);
#endif
}

// Returns true when stdin is connected to a terminal (not a pipe or redirect).
// Used to decide whether to show the interactive arrow-key permission prompt.
bool stdinIsInteractive() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(fileno(stdin)) != 0;
#endif
}

// \r  — carriage return (move cursor to column 0).
// \033[2K — CSI 2K: erase entire current line.
void clearLine() {
    std::cout << "\r\033[2K";
}

// \033[N]A — CSI N A: move cursor up N lines (CUU — Cursor Up).
void moveUp(int lines) {
    if (lines > 0) {
        std::cout << "\033[" << lines << "A";
    }
}

int readChoiceKey() {
#ifdef _WIN32
    const int ch = _getch();
    // 0x00 / 0xE0 (224): Windows extended-key prefix for arrow keys, function keys, etc.
    if (ch == 0 || ch == 224) {
        const int extended = _getch();
        // 72 = VK_UP (↑),  75 = VK_LEFT  (←) — navigate toward "yes" (index -1).
        if (extended == 72 || extended == 75) {
            return -1;
        }
        // 80 = VK_DOWN (↓), 77 = VK_RIGHT (→) — navigate toward "no"  (index +1).
        if (extended == 80 || extended == 77) {
            return 1;
        }
        return 0;
    }
    return ch;
#else
    // Save current terminal settings.
    termios oldSettings{};
    if (tcgetattr(STDIN_FILENO, &oldSettings) != 0) {
        return std::cin.get();
    }

    // Switch to raw (non-canonical, no-echo) mode so we can read keystrokes
    // one at a time without waiting for Enter.
    termios raw = oldSettings;
    raw.c_lflag &= static_cast<unsigned>(~(ICANON | ECHO));
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    const int ch = std::cin.get();
    int result = ch;
    // ESC (27) + '[' is the CSI (Control Sequence Introducer) for ANSI escape
    // sequences. Arrow keys send ESC [ A / B / C / D.
    if (ch == 27 && std::cin.peek() == '[') {
        std::cin.get();               // consume '['
        const int arrow = std::cin.get();
        // 'A' = Up,  'D' = Left  — navigate toward "yes" (-1).
        if (arrow == 'A' || arrow == 'D') {
            result = -1;
        // 'B' = Down, 'C' = Right — navigate toward "no"  (+1).
        } else if (arrow == 'B' || arrow == 'C') {
            result = 1;
        }
    }

    // Restore original terminal settings before returning.
    tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
    return result;
#endif
}

}  // namespace

Console::Console(bool enableColor, bool typewriter)
    : color_(enableColor), typewriter_(typewriter) {}

// Returns the ANSI escape code if color is enabled, or an empty string otherwise.
// This null-object pattern avoids littering every print method with `if (color_)` guards.
std::string Console::color(const std::string& code) const {
    return color_ ? code : "";
}

void Console::printBanner(const std::string& model, const std::string& workspace) const {
    std::cout << color(bold) << "cpp-ai-agent" << color(reset) << "\n";
    std::cout << color(gray) << "LLM " << color(reset) << model
              << color(gray) << "  |  workspace: " << color(reset) << workspace << "\n\n";
}

// Prints text with a 2-space left indent. Splits on '\n' so that multi-line
// content stays uniformly indented regardless of embedded newlines.
void Console::printIndented(const std::string& text) const {
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        const auto line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        std::cout << "  " << line << "\n";
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
}

void Console::printUser(const std::string& text) const {
    std::cout << color(cyan) << "▸ you" << color(reset) << "\n";
    printIndented(text);
    std::cout << "\n";
}

void Console::printAssistant(const std::string& text) const {
    std::cout << color(bold) << "▸ assistant" << color(reset) << "\n  ";
    if (!typewriter_) {
        std::cout << text << "\n\n";
        return;
    }

    // Pseudo-streaming typewriter loop: output the assistant reply in small
    // chunks spaced by stepDelayMs_, respecting UTF-8 code-point boundaries
    // so that multi-byte characters are never split mid-sequence.
    std::size_t i = 0;
    int printedChars = 0;
    while (i < text.size()) {
        // After maxTypewriterChars_ characters the effect has served its
        // purpose — dump the remainder instantly to avoid dragging.
        if (printedChars >= maxTypewriterChars_) {
            std::cout << text.substr(i);
            break;
        }

        // Advance by exactly charsPerStep_ complete UTF-8 code points.
        int stepChars = 0;
        const auto start = i;
        while (i < text.size() && stepChars < charsPerStep_) {
            const auto len = utf8CharLen(static_cast<unsigned char>(text[i]));
            i += std::min<std::size_t>(len, text.size() - i);
            ++stepChars;
            ++printedChars;
        }

        std::cout << text.substr(start, i - start) << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(stepDelayMs_));
    }
    std::cout << "\n\n";
}

void Console::printToolCall(const std::string& name, const std::string& args, const std::string& risk) const {
    std::cout << color(blue) << "● " << name << color(reset)
              << color(gray) << "  " << risk << color(reset) << "\n";
    printIndented(args);
}

void Console::printToolResult(const std::string& name, const std::string& detail) const {
    std::cout << color(gray) << "└ " << name << ": " << color(reset) << detail << "\n\n";
}

void Console::printPermissionPrompt(
    const std::string& toolName,
    const std::string& risk,
    const std::string& args,
    const std::string& preview
) const {
    std::cout << "\n" << color(yellow) << "! permission required" << color(reset) << "\n";
    std::cout << "  tool: " << toolName << "  risk: " << risk << "\n";
    std::cout << "  args: " << args << "\n";
    if (!preview.empty()) {
        std::cout << color(gray);
        printIndented(preview);
        std::cout << color(reset);
    }
    std::cout << "\n";
    std::cout << "  Use arrow keys to choose, then press Enter.\n";
}

bool Console::confirmPermission(
    const std::string& toolName,
    const std::string& risk,
    const std::string& args,
    const std::string& preview
) const {
    printPermissionPrompt(toolName, risk, args, preview);

    // Non-interactive fallback (piped stdin): use plain line-input prompt.
    if (!stdinIsInteractive()) {
        std::cout << color(yellow) << "  choose [y/N]> " << color(reset);
        std::string answer;
        if (!std::getline(std::cin, answer)) {
            return false;
        }
        return answer == "y" || answer == "yes";
    }

    // Interactive terminal: render an in-place two-option menu.
    // selected=0 → "yes", selected=1 → "no" (default).
    // Arrow keys and Tab toggle the selection; Enter confirms; y/n/Esc are
    // shortcuts. The menu is rendered by overwriting the same two screen
    // lines via clearLine() + moveUp() so the display stays clean.
    int selected = 1;
    auto renderOptions = [&]() {
        const auto marker = [&](int index, const std::string& label) {
            if (selected == index) {
                return color(inverse) + "> " + label + color(reset);
            }
            return std::string("  ") + label;
        };

        clearLine();
        std::cout << "  " << marker(0, "[y] yes  apply this change") << "\n";
        clearLine();
        std::cout << "  " << marker(1, "[n] no   reject this change") << "\n";
        std::cout.flush();
    };

    renderOptions();
    while (true) {
        const int key = readChoiceKey();
        if (key == -1 || key == 1 || key == '\t') {
            selected = 1 - selected;
            moveUp(2);
            renderOptions();
            continue;
        }
        if (key == 'y' || key == 'Y') {
            selected = 0;
            moveUp(2);
            renderOptions();
            std::cout << "\n";
            return true;
        }
        if (key == 'n' || key == 'N' || key == 27) {
            selected = 1;
            moveUp(2);
            renderOptions();
            std::cout << "\n";
            return false;
        }
        if (key == '\r' || key == '\n') {
            std::cout << "\n";
            return selected == 0;
        }
    }
}

void Console::printWarning(const std::string& text) const {
    std::cout << color(yellow) << "! " << color(reset) << text << "\n\n";
}

void Console::printError(const std::string& text) const {
    std::cout << color(red) << "error> " << color(reset) << text << "\n";
}

void Console::printUiOverview(
    const std::string& model,
    const std::string& baseUrl,
    const std::string& workspace,
    const std::string& historyDir
) const {
    printBanner(model, workspace);
    std::cout << color(cyan) << "▸ Conversation" << color(reset) << "\n";
    std::cout << "  Streamed user and assistant turns appear here.\n\n";
    std::cout << color(blue) << "● Tools" << color(reset) << "\n";
    std::cout << "  read_file safe | write_file diff+confirm | edit_file diff+confirm | run_command guarded\n\n";
    std::cout << color(gray) << "Status" << color(reset) << "\n";
    std::cout << "  API: " << stripScheme(baseUrl) << "\n";
    std::cout << "  History: " << historyDir << "\n\n";
    std::cout << "› Type a message. /exit quits.\n";
}

// Returns true when stdout is connected to a terminal (not piped to a file).
// When false, the typewriter effect and ANSI colors should degrade gracefully.
bool detectInteractiveOutput() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

bool detectColorSupport() {
    if (!readEnv("NO_COLOR").empty()) {
        return false;
    }

    const auto term = readEnv("TERM");
    if (term == "dumb") {
        return false;
    }

    if (!detectInteractiveOutput()) {
        return false;
    }

#ifdef _WIN32
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD mode = 0;
    if (GetConsoleMode(output, &mode) == 0) {
        return false;
    }

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(output, mode) != 0) {
        return true;
    }

    // SetConsoleMode failed — fall back to TERM check for third-party terminal
    // emulators (e.g. ansicon, ConEmu, Cmder) that set TERM=xterm or similar.
    // These emulators provide ANSI processing themselves, so the Windows console
    // mode doesn't matter.
    const auto term = readEnv("TERM");
    if (!term.empty() && term != "dumb") {
        return true;
    }

    return false;
#else
    return true;
#endif
}

}  // namespace cpp_ai_agent::ui
