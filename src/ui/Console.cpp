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

constexpr const char* reset = "\033[0m";
constexpr const char* bold = "\033[1m";
constexpr const char* cyan = "\033[36m";
constexpr const char* blue = "\033[34m";
constexpr const char* gray = "\033[90m";
constexpr const char* yellow = "\033[33m";
constexpr const char* red = "\033[31m";
constexpr const char* inverse = "\033[7m";

std::size_t utf8CharLen(unsigned char firstByte) {
    if (firstByte < 0x80) {
        return 1;
    }
    if ((firstByte >> 5) == 0x06) {
        return 2;
    }
    if ((firstByte >> 4) == 0x0E) {
        return 3;
    }
    if ((firstByte >> 3) == 0x1E) {
        return 4;
    }
    return 1;
}

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

bool stdinIsInteractive() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(fileno(stdin)) != 0;
#endif
}

void clearLine() {
    std::cout << "\r\033[2K";
}

void moveUp(int lines) {
    if (lines > 0) {
        std::cout << "\033[" << lines << "A";
    }
}

int readChoiceKey() {
#ifdef _WIN32
    const int ch = _getch();
    if (ch == 0 || ch == 224) {
        const int extended = _getch();
        if (extended == 72 || extended == 75) {
            return -1;
        }
        if (extended == 80 || extended == 77) {
            return 1;
        }
        return 0;
    }
    return ch;
#else
    termios oldSettings{};
    if (tcgetattr(STDIN_FILENO, &oldSettings) != 0) {
        return std::cin.get();
    }

    termios raw = oldSettings;
    raw.c_lflag &= static_cast<unsigned>(~(ICANON | ECHO));
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    const int ch = std::cin.get();
    int result = ch;
    if (ch == 27 && std::cin.peek() == '[') {
        std::cin.get();
        const int arrow = std::cin.get();
        if (arrow == 'A' || arrow == 'D') {
            result = -1;
        } else if (arrow == 'B' || arrow == 'C') {
            result = 1;
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
    return result;
#endif
}

}  // namespace

Console::Console(bool enableColor, bool typewriter)
    : color_(enableColor), typewriter_(typewriter) {}

std::string Console::color(const std::string& code) const {
    return color_ ? code : "";
}

void Console::printBanner(const std::string& model, const std::string& workspace) const {
    std::cout << color(bold) << "cpp-ai-agent" << color(reset) << "\n";
    std::cout << color(gray) << "LLM " << color(reset) << model
              << color(gray) << "  |  workspace: " << color(reset) << workspace << "\n\n";
}

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

    std::size_t i = 0;
    int printedChars = 0;
    while (i < text.size()) {
        if (printedChars >= maxTypewriterChars_) {
            std::cout << text.substr(i);
            break;
        }

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

    if (!stdinIsInteractive()) {
        std::cout << color(yellow) << "  choose [y/N]> " << color(reset);
        std::string answer;
        if (!std::getline(std::cin, answer)) {
            return false;
        }
        return answer == "y" || answer == "yes";
    }

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
    return SetConsoleMode(output, mode) != 0;
#else
    return true;
#endif
}

}  // namespace cpp_ai_agent::ui
