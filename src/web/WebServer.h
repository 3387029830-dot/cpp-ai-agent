#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace cpp_ai_agent::web {

struct WebEvent {
    int id = 0;
    std::string type;
    std::string title;
    std::string detail;
    nlohmann::json data = nlohmann::json::object();
};

class WebServer {
public:
    using ChatHandler = std::function<void(const std::string& content)>;
    using SettingsHandler = std::function<nlohmann::json(const nlohmann::json& body)>;

    WebServer(std::string webRoot, std::string workspaceRoot = ".");

    void setChatHandler(ChatHandler handler);
    void setSettingsHandler(SettingsHandler handler);
    void setStatusPayload(nlohmann::json payload);
    void pushEvent(std::string type, std::string title, std::string detail, nlohmann::json data = nlohmann::json::object());
    void setBusy(bool busy);
    bool isBusy() const;

    bool requestPermission(
        const std::string& toolName,
        const std::string& risk,
        const std::string& arguments,
        const std::string& preview
    );

    int start(int port);

private:
    std::string eventsAfter(int lastId) const;
    std::string statusJson() const;
    std::string uploadFile(const nlohmann::json& body);
    bool resolvePermission(bool approved);
    void handleClient(std::uintptr_t clientSocket);

    std::string webRoot_;
    std::string workspaceRoot_;
    ChatHandler chatHandler_;
    SettingsHandler settingsHandler_;
    nlohmann::json statusPayload_ = nlohmann::json::object();

    mutable std::mutex mutex_;
    std::vector<WebEvent> events_;
    int nextEventId_ = 1;
    bool busy_ = false;

    mutable std::mutex permissionMutex_;
    std::condition_variable permissionCv_;
    int permissionId_ = 0;
    std::optional<bool> permissionResult_;
};

}  // namespace cpp_ai_agent::web
