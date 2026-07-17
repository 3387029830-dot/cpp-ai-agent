#include "web/WebServer.h"
#include "storage/HistoryReader.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
constexpr SocketHandle INVALID_SOCK = INVALID_SOCKET;
#define SOCKET_ERR SOCKET_ERROR
#define CLOSE_SOCK closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using SocketHandle = int;
constexpr SocketHandle INVALID_SOCK = -1;
#define SOCKET_ERR (-1)
#define CLOSE_SOCK close
#endif

namespace cpp_ai_agent::web {

namespace {

std::string jsonResponse(const std::string& body, const std::string& status = "200 OK") {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << "\r\n"
        << "Content-Type: application/json; charset=utf-8\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Cache-Control: no-store\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    return out.str();
}

std::string dumpJson(const nlohmann::json& value) {
    return value.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::string htmlResponse(const std::string& body, const std::string& contentType = "text/html; charset=utf-8") {
    std::ostringstream out;
    out << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Cache-Control: no-store\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    return out.str();
}

std::string notFoundResponse() {
    return jsonResponse(R"({"error":"not found"})", "404 Not Found");
}

std::string badRequestResponse(const std::string& message) {
    return jsonResponse(dumpJson({{"error", message}}), "400 Bad Request");
}

std::string busyResponse() {
    return jsonResponse(R"({"error":"agent is busy"})", "409 Conflict");
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string contentTypeFor(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    if (ext == ".css") {
        return "text/css; charset=utf-8";
    }
    if (ext == ".js") {
        return "application/javascript; charset=utf-8";
    }
    if (ext == ".svg") {
        return "image/svg+xml";
    }
    return "text/html; charset=utf-8";
}

std::string urlDecode(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const auto hex = value.substr(i + 1, 2);
            char* end = nullptr;
            const auto decoded = static_cast<char>(std::strtol(hex.c_str(), &end, 16));
            if (end != nullptr && *end == '\0') {
                out.push_back(decoded);
                i += 2;
            }
        } else if (value[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(value[i]);
        }
    }
    return out;
}

int queryInt(const std::string& target, const std::string& key, int fallback) {
    const auto queryPos = target.find('?');
    if (queryPos == std::string::npos) {
        return fallback;
    }
    const auto query = target.substr(queryPos + 1);
    std::istringstream parts(query);
    std::string part;
    while (std::getline(parts, part, '&')) {
        const auto eq = part.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        if (urlDecode(part.substr(0, eq)) == key) {
            try {
                return std::stoi(urlDecode(part.substr(eq + 1)));
            } catch (const std::exception&) {
                return fallback;
            }
        }
    }
    return fallback;
}

std::string routePath(const std::string& target) {
    const auto queryPos = target.find('?');
    return queryPos == std::string::npos ? target : target.substr(0, queryPos);
}

std::string safeFileName(std::string value) {
    if (value.empty() || value == "." || value == "..") {
        return "upload.txt";
    }

    for (auto& ch : value) {
        const auto uch = static_cast<unsigned char>(ch);
        if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' ||
            ch == '"' || ch == '<' || ch == '>' || ch == '|' || uch < 32) {
            ch = '_';
        }
    }
    return value;
}

void sendAll(std::uintptr_t rawSocket, const std::string& data) {
    const auto socket = static_cast<SocketHandle>(rawSocket);
    const char* ptr = data.data();
    int remaining = static_cast<int>(data.size());
    while (remaining > 0) {
        const int sent = send(socket, ptr, remaining, 0);
        if (sent <= 0) {
            return;
        }
        ptr += sent;
        remaining -= sent;
    }
}

}  // namespace

WebServer::WebServer(std::string webRoot, std::string workspaceRoot)
    : webRoot_(std::move(webRoot)), workspaceRoot_(std::move(workspaceRoot)) {}

void WebServer::setHistoryDir(std::string dir) {
    historyDir_ = std::move(dir);
}

void WebServer::setChatHandler(ChatHandler handler) {
    chatHandler_ = std::move(handler);
}

void WebServer::setSettingsHandler(SettingsHandler handler) {
    settingsHandler_ = std::move(handler);
}

void WebServer::setStatusPayload(nlohmann::json payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    statusPayload_ = std::move(payload);
}

void WebServer::pushEvent(std::string type, std::string title, std::string detail, nlohmann::json data) {
    std::lock_guard<std::mutex> lock(mutex_);
    WebEvent event;
    event.id = nextEventId_++;
    event.type = std::move(type);
    event.title = std::move(title);
    event.detail = std::move(detail);
    event.data = std::move(data);
    events_.push_back(std::move(event));
    if (events_.size() > 600) {
        events_.erase(events_.begin(), events_.begin() + 100);
    }
}

void WebServer::setBusy(bool busy) {
    std::lock_guard<std::mutex> lock(mutex_);
    busy_ = busy;
}

bool WebServer::isBusy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return busy_;
}

bool WebServer::requestPermission(
    const std::string& toolName,
    const std::string& risk,
    const std::string& arguments,
    const std::string& preview
) {
    int id = 0;
    {
        std::lock_guard<std::mutex> lock(permissionMutex_);
        id = ++permissionId_;
        permissionResult_.reset();
    }

    pushEvent(
        "permission",
        toolName,
        preview,
        {
            {"request_id", id},
            {"tool", toolName},
            {"risk", risk},
            {"arguments", arguments},
            {"preview", preview},
        }
    );

    std::unique_lock<std::mutex> lock(permissionMutex_);
    const auto ready = permissionCv_.wait_for(lock, std::chrono::minutes(5), [this] {
        return permissionResult_.has_value();
    });
    if (!ready) {
        return false;
    }
    const bool approved = *permissionResult_;
    permissionResult_.reset();
    return approved;
}

bool WebServer::resolvePermission(bool approved) {
    {
        std::lock_guard<std::mutex> lock(permissionMutex_);
        permissionResult_ = approved;
    }
    permissionCv_.notify_one();
    pushEvent("permission_result", approved ? "approved" : "denied", approved ? "approved" : "denied");
    return true;
}

std::string WebServer::eventsAfter(int lastId) const {
    nlohmann::json items = nlohmann::json::array();
    bool busy = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        busy = busy_;
        for (const auto& event : events_) {
            if (event.id <= lastId) {
                continue;
            }
            items.push_back({
                {"id", event.id},
                {"type", event.type},
                {"title", event.title},
                {"detail", event.detail},
                {"data", event.data},
            });
        }
    }
    return dumpJson({{"events", items}, {"busy", busy}});
}

std::string WebServer::statusJson() const {
    nlohmann::json payload;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        payload = statusPayload_;
    }
    payload["name"] = "cpp-ai-agent";
    payload["mode"] = "web";
    payload["busy"] = isBusy();
    return dumpJson(payload);
}

std::string WebServer::uploadFile(const nlohmann::json& body) {
    const auto filename = safeFileName(body.value("filename", "upload.txt"));
    const auto content = body.value("content", "");
    const auto uploadRoot = std::filesystem::path(workspaceRoot_) / "uploads";

    std::error_code ec;
    std::filesystem::create_directories(uploadRoot, ec);
    if (ec) {
        throw std::runtime_error("failed to create uploads directory: " + ec.message());
    }

    const auto outputPath = uploadRoot / filename;
    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to write upload file");
    }
    output << content;

    const auto relative = (std::filesystem::path("uploads") / filename).generic_string();
    pushEvent(
        "upload",
        filename,
        relative,
        {
            {"filename", filename},
            {"path", relative},
            {"bytes", content.size()},
        }
    );
    return dumpJson({{"ok", true}, {"path", relative}, {"bytes", content.size()}});
}

void WebServer::handleClient(std::uintptr_t clientSocket) {
    const SocketHandle sock = static_cast<SocketHandle>(clientSocket);
    std::string request;
    std::array<char, 4096> buffer{};
    int received = 0;
    while ((received = recv(sock, buffer.data(), static_cast<int>(buffer.size()), 0)) > 0) {
        request.append(buffer.data(), static_cast<std::size_t>(received));
        const auto headerEnd = request.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            auto contentLength = 0;
            const auto header = request.substr(0, headerEnd);
            std::istringstream headerStream(header);
            std::string line;
            while (std::getline(headerStream, line)) {
                const std::string key = "Content-Length:";
                if (line.rfind(key, 0) == 0) {
                    contentLength = std::stoi(line.substr(key.size()));
                }
            }
            if (request.size() >= headerEnd + 4 + static_cast<std::size_t>(contentLength)) {
                break;
            }
        }
    }

    std::istringstream stream(request);
    std::string method;
    std::string target;
    std::string version;
    stream >> method >> target >> version;

    const auto headerEnd = request.find("\r\n\r\n");
    const auto body = headerEnd == std::string::npos ? std::string() : request.substr(headerEnd + 4);
    const auto path = routePath(target);

    try {
        if (method == "GET" && path == "/api/events") {
            sendAll(clientSocket, jsonResponse(eventsAfter(queryInt(target, "after", 0))));
        } else if (method == "GET" && path == "/api/status") {
            sendAll(clientSocket, jsonResponse(statusJson()));
        } else if (method == "POST" && path == "/api/chat") {
            if (isBusy()) {
                sendAll(clientSocket, busyResponse());
            } else if (!chatHandler_) {
                sendAll(clientSocket, badRequestResponse("chat handler is not configured"));
            } else {
                const auto parsed = nlohmann::json::parse(body.empty() ? "{}" : body);
                const auto content = parsed.value("content", "");
                if (content.empty()) {
                    sendAll(clientSocket, badRequestResponse("content is required"));
                } else {
                    setBusy(true);
                    pushEvent("user", "you", content);
                    auto handler = chatHandler_;
                    std::thread([this, handler, content] {
                        try {
                            handler(content);
                        } catch (const std::exception& ex) {
                            pushEvent("error", "agent", ex.what());
                        }
                        setBusy(false);
                        pushEvent("turn_done", "agent", "done");
                    }).detach();
                    sendAll(clientSocket, jsonResponse(R"({"accepted":true})"));
                }
            }
        } else if (method == "POST" && path == "/api/permission") {
            const auto parsed = nlohmann::json::parse(body.empty() ? "{}" : body);
            resolvePermission(parsed.value("approved", false));
            sendAll(clientSocket, jsonResponse(R"({"ok":true})"));
        } else if (method == "POST" && path == "/api/settings") {
            if (!settingsHandler_) {
                sendAll(clientSocket, badRequestResponse("settings handler is not configured"));
            } else {
                const auto parsed = nlohmann::json::parse(body.empty() ? "{}" : body);
                sendAll(clientSocket, jsonResponse(dumpJson(settingsHandler_(parsed))));
            }
        } else if (method == "POST" && path == "/api/upload") {
            const auto parsed = nlohmann::json::parse(body.empty() ? "{}" : body);
            sendAll(clientSocket, jsonResponse(uploadFile(parsed)));
        } else if (method == "GET" && path == "/api/history") {
            nlohmann::json list = nlohmann::json::array();
            if (!historyDir_.empty()) {
                try {
                    for (const auto& entry : cpp_ai_agent::storage::listHistoryFiles(historyDir_)) {
                        list.push_back({
                            {"name", entry.path.filename().string()},
                            {"size", entry.size},
                            {"modified", entry.modifiedTime},
                            {"title", entry.title},
                        });
                    }
                } catch (const std::exception& ex) {
                    list = nlohmann::json::array();
                }
            }
            sendAll(clientSocket, jsonResponse(dumpJson(list)));
        } else if (method == "GET") {
            if (path.find("..") != std::string::npos || path.find('\\') != std::string::npos) {
                sendAll(clientSocket, notFoundResponse());
                CLOSE_SOCK(sock);
                return;
            }
            auto filePath = path == "/" ? std::filesystem::path(webRoot_) / "index.html"
                                        : std::filesystem::path(webRoot_) / path.substr(1);
            const auto content = readFile(filePath);
            if (content.empty()) {
                sendAll(clientSocket, notFoundResponse());
            } else {
                sendAll(clientSocket, htmlResponse(content, contentTypeFor(filePath)));
            }
        } else {
            sendAll(clientSocket, notFoundResponse());
        }
    } catch (const std::exception& ex) {
        sendAll(clientSocket, badRequestResponse(ex.what()));
    }

    CLOSE_SOCK(sock);
}

int WebServer::start(int port) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
#endif

    SocketHandle serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCK) {
#ifdef _WIN32
        WSACleanup();
#endif
        throw std::runtime_error("socket failed");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<u_short>(port));

    int yes = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERR) {
        CLOSE_SOCK(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        throw std::runtime_error("bind failed; port may already be in use");
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERR) {
        CLOSE_SOCK(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        throw std::runtime_error("listen failed");
    }

    std::cout << "web> listening on http://127.0.0.1:" << port << "\n";
    std::cout << "web> press Ctrl+C to stop\n";

    while (true) {
        SocketHandle client = accept(serverSocket, nullptr, nullptr);
        if (client == INVALID_SOCK) {
            continue;
        }
        std::thread([this, client] {
            handleClient(static_cast<std::uintptr_t>(client));
        }).detach();
    }
}

}  // namespace cpp_ai_agent::web
