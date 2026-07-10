#pragma once

#include <string>

namespace cpp_ai_agent::ui {

class Console {
public:
    Console(bool enableColor, bool typewriter);

    void printBanner(const std::string& model, const std::string& workspace) const;
    void printUser(const std::string& text) const;
    void printAssistant(const std::string& text) const;
    void printAssistantChunk(const std::string& chunk);  // auto-header on first call
    void finishAssistantStream();                         // close streaming: footer + reset
    void printToolCall(const std::string& name, const std::string& args, const std::string& risk) const;
    void printToolResult(const std::string& name, const std::string& detail) const;
    void printPermissionPrompt(
        const std::string& toolName,
        const std::string& risk,
        const std::string& args,
        const std::string& preview
    ) const;
    bool confirmPermission(
        const std::string& toolName,
        const std::string& risk,
        const std::string& args,
        const std::string& preview
    ) const;
    void printWarning(const std::string& text) const;
    void printError(const std::string& text) const;
    void printUiOverview(const std::string& model, const std::string& baseUrl, const std::string& workspace, const std::string& historyDir) const;

private:
    std::string color(const std::string& code) const;
    void printIndented(const std::string& text) const;
    void printAssistantHeader() const;
    void printAssistantFooter() const;

    bool color_ = false;
    bool typewriter_ = false;
    bool streaming_ = false;  // track streaming state for printAssistantChunk
    int charsPerStep_ = 3;
    int stepDelayMs_ = 12;
    int maxTypewriterChars_ = 1200;
    int streamCharDelayMs_ = 8;   // per-char delay (ms) for smooth streaming output
};

bool detectColorSupport();
bool detectInteractiveOutput();

}  // namespace cpp_ai_agent::ui
