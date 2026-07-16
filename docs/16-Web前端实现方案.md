# cpp-ai-agent Web 前端实现方案

> 目标：在现有 Console UI 之外新增 Web 浏览器界面，两种模式共享全部底层代码
> 日期：2026-07-15

---

## 一、架构概览

```
浏览器 (web/index.html)
    │  HTTP POST /api/chat        ← 用户发送消息
    │  WebSocket /ws               ← 流式接收 assistant 回复
    ▼
cpp-httplib (单头文件 HTTP/WS 库)
    │  src/server/HttpServer.cpp
    │  src/server/WebUi.cpp
    ▼
AgentLoop / LlmClient / ToolRegistry / JsonLogger / PermissionManager
    (全部不变)
```

运行时两种模式选择：

```cpp
// config/settings.json
{
  "ui": {
    "mode": "console"   // "console" | "web"
  }
}
```

```
ai-agent.exe               → 默认 console 模式，跟现在一样
ai-agent.exe --ui web      → 启动 Web 服务器，浏览器访问
```

---

## 二、文件清单

### 新增文件

| 文件 | 行数 | 职责 |
|------|------|------|
| `src/ui/IUi.h` | ~40 | 纯虚接口，定义所有 UI 方法 |
| `src/server/httplib.h` | ~8000 | cpp-httplib 单头文件（从 GitHub 复制） |
| `src/server/HttpServer.h` | ~60 | HTTP Server 封装：路由、WebSocket、线程管理 |
| `src/server/HttpServer.cpp` | ~200 | 实现：消息队列、Session 管理、事件广播 |
| `src/server/WebUi.h` | ~50 | 实现 IUi 接口，将 AgentEvent 转为 WebSocket 推送 |
| `src/server/WebUi.cpp` | ~150 | 具体实现 |
| `web/index.html` | ~300 | 前端单页面：聊天气泡 + 输入框 + 工具日志 |
| `web/style.css` | ~100 | 前端样式（可选，也可内联在 HTML 中） |

### 修改文件

| 文件 | 改动 |
|------|------|
| `src/ui/Console.h` | `class Console : public IUi` |
| `src/main.cpp` | `unique_ptr<IUi>` 替代 `Console`，加 `--ui` 参数 |
| `config/settings.json` | 新增 `ui.mode` 字段 |
| `CMakeLists.txt` | 新增 `src/server/HttpServer.cpp` `src/server/WebUi.cpp` |

---

## 三、IUi 接口定义

`src/ui/IUi.h`:

```cpp
#pragma once
#include "agent/AgentLoop.h"  // AgentEvent
#include "security/PermissionManager.h"  // PermissionRequest
#include <string>

namespace cpp_ai_agent::ui {

class IUi {
public:
    virtual ~IUi() = default;

    // 启动横幅
    virtual void printBanner(const std::string& model,
                             const std::string& workspace) = 0;

    // 流式 assistant 回复
    virtual void printAssistantChunk(const std::string& chunk) = 0;
    virtual void finishAssistantStream() = 0;

    // 非流式 assistant 回复
    virtual void printAssistant(const std::string& text) = 0;

    // 工具调用与结果
    virtual void printToolCall(const std::string& name,
                               const std::string& args,
                               const std::string& risk) = 0;
    virtual void printToolResult(const std::string& name,
                                 const std::string& detail) = 0;

    // 警告和错误
    virtual void printWarning(const std::string& text) = 0;
    virtual void printError(const std::string& text) = 0;

    // 权限确认（阻塞等待用户选择）
    virtual bool confirmPermission(
        const std::string& toolName,
        const std::string& risk,
        const std::string& args,
        const std::string& preview) = 0;

    // 运行主循环（Console 模式是 getline 循环，Web 模式是 HTTP 监听）
    // 返回 false 表示退出
    virtual bool runLoop(const std::string& input) = 0;
};

} // namespace
```

**关键设计决策**：
- `confirmPermission()` — Web 模式下需要阻塞等待用户在浏览器中点击 yes/no，实现方式见后文
- `runLoop()` — Console 模式内嵌 getline；Web 模式不阻塞，由 HTTP handler 驱动

---

## 四、WebUi 实现流程

### 4.1 整体数据流

```
浏览器                         C++ WebUi                     AgentLoop
  │                                │                              │
  │  POST /api/chat                │                              │
  │  {"content":"读 README.md"}    │                              │
  │ ──────────────────────────────>│                              │
  │                                │  session.addMessage(User)    │
  │                                │ ────────────────────────────>│
  │                                │                              │
  │                                │  agentLoop.runTurn(session)  │
  │                                │ ────────────────────────────>│
  │                                │                              │
  │                                │   onEvent:                   │
  │                                │   AssistantChunk("我来")     │
  │                                │ <────────────────────────────│
  │   WS: {"type":"chunk",        │                              │
  │        "data":"我来"}          │                              │
  │ <──────────────────────────────│                              │
  │                                │                              │
  │   WS: {"type":"chunk",        │                              │
  │        "data":"读取"}          │                              │
  │ <──────────────────────────────│                              │
  │                                │                              │
  │                                │   onEvent:                   │
  │                                │   ToolCall("read_file",...)  │
  │                                │ <────────────────────────────│
  │   WS: {"type":"tool_call",    │                              │
  │        "name":"read_file",     │                              │
  │        "args":"README.md"}     │                              │
  │ <──────────────────────────────│                              │
  │                                │                              │
  │   WS: {"type":"tool_result",  │                              │
  │        "name":"read_file",     │                              │
  │        "data":"# cpp-ai..."}   │                              │
  │ <──────────────────────────────│                              │
  │                                │                              │
  │   WS: {"type":"done"}         │                              │
  │ <──────────────────────────────│                              │
```

### 4.2 HttpServer 核心数据结构

```cpp
// HttpServer.h
namespace cpp_ai_agent::server {

class HttpServer {
public:
    HttpServer(int port, IUi& ui, core::Session& session, agent::AgentLoop& agent);

    void start();       // 启动 HTTP 服务（阻塞当前线程）
    void stop();        // 停止服务

    // 权限确认同步机制
    bool waitForPermission(const PermissionRequest& req);

private:
    int port_;
    IUi& ui_;
    core::Session& session_;
    agent::AgentLoop& agent_;

    // WebSocket 连接列表（广播 assistant chunk 用）
    std::vector<websocket_connection> ws_clients_;

    // 权限确认同步：用户点击 yes/no 后通知阻塞的 AgentLoop 线程
    std::mutex permission_mutex_;
    std::condition_variable permission_cv_;
    std::optional<bool> permission_result_;
};

} // namespace
```

### 4.3 路由设计

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/` | 返回 `web/index.html` |
| GET | `/style.css` | 返回样式文件 |
| POST | `/api/chat` | 接收用户消息 JSON `{"content":"..."}` |
| WS | `/ws` | WebSocket 连接，推送流式事件 |
| POST | `/api/permission` | 权限确认回传 `{"approved": true/false}` |

### 4.4 核心流程伪代码

#### 用户发送消息

```cpp
// POST /api/chat handler
server.Post("/api/chat", [&](const Request& req, Response& res) {
    auto json = nlohmann::json::parse(req.body);
    std::string content = json["content"];

    // 1. 添加到 Session
    session_.addMessage(User, content);
    logger_.log("user_message", {{"content", content}});

    // 2. 在新的线程中运行 AgentLoop（不阻塞 HTTP 响应）
    std::thread([&]() {
        agentLoop_.runTurn(session_);
        // AgentLoop 内部回调 IUi 方法，
        // WebUi 实现将事件通过 WebSocket 广播
    }).detach();

    // 3. 立即返回
    res.set_content("{}", "application/json");
});
```

#### AgentEvent → WebSocket 广播

```cpp
// WebUi.cpp — 实现 IUi::printAssistantChunk()
void WebUi::printAssistantChunk(const std::string& chunk) {
    nlohmann::json msg = {
        {"type", "chunk"},
        {"data", chunk}
    };
    server_.broadcast_ws(msg.dump());
}

void WebUi::printToolCall(const std::string& name,
                           const std::string& args,
                           const std::string& risk) {
    nlohmann::json msg = {
        {"type", "tool_call"},
        {"name", name},
        {"args", args},
        {"risk", risk}
    };
    server_.broadcast_ws(msg.dump());
}

void WebUi::finishAssistantStream() {
    nlohmann::json msg = {{"type", "done"}};
    server_.broadcast_ws(msg.dump());
}
```

#### 权限确认（Web 模式需要同步等待）

```cpp
// WebUi.cpp
bool WebUi::confirmPermission(const std::string& toolName,
                               const std::string& risk,
                               const std::string& args,
                               const std::string& preview) {
    // 1. 通过 WebSocket 推送权限请求到浏览器
    nlohmann::json msg = {
        {"type", "permission"},
        {"tool", toolName},
        {"risk", risk},
        {"args", args},
        {"preview", preview}
    };
    server_.broadcast_ws(msg.dump());

    // 2. 阻塞等待用户点击 yes/no
    return server_.waitForPermission();
}

// HttpServer.cpp
bool HttpServer::waitForPermission() {
    std::unique_lock<std::mutex> lock(permission_mutex_);
    permission_cv_.wait(lock, [this] {
        return permission_result_.has_value();
    });
    bool result = *permission_result_;
    permission_result_.reset();
    return result;
}

// POST /api/permission handler（浏览器 yes/no 点击后调用）
server.Post("/api/permission", [&](const Request& req, Response& res) {
    auto json = nlohmann::json::parse(req.body);
    {
        std::lock_guard<std::mutex> lock(permission_mutex_);
        permission_result_ = json["approved"].get<bool>();
    }
    permission_cv_.notify_one();
    res.set_content("{}", "application/json");
});
```

---

## 五、前端设计（web/index.html）

### 5.1 布局

```
┌─────────────────────────────────────────────────┐
│  cpp-ai-agent Web Console          [● connected] │
├─────────────────────────────────────────────────┤
│                                                  │
│  ▸ assistant                                    │
│  我来帮你读取 README.md 文件...                   │
│                                                  │
│  ● read_file  safe                              │
│    {"path": "README.md"}                         │
│  └ read_file: # cpp-ai-agent\n\n一个 C++17...   │
│                                                  │
│  ▸ you                                          │
│  请总结这个项目                                  │
│                                                  │
│  ▸ assistant (流式逐字出现...)                    │
│  这个项目是一个用 C++17 实现的终端 AI...           │
│                                                  │
├─────────────────────────────────────────────────┤
│ [输入消息...                          ] [发送]   │
└─────────────────────────────────────────────────┘
```

### 5.2 核心 HTML 结构

```html
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>cpp-ai-agent</title>
  <style>
    /* 见 style.css */
  </style>
</head>
<body>
  <div id="header">
    <h1>cpp-ai-agent</h1>
    <span id="status">● connected</span>
  </div>

  <div id="chat">
    <!-- 消息动态插入此处 -->
  </div>

  <div id="permission-dialog" style="display:none">
    <!-- 权限确认弹窗 -->
    <div class="dialog-content">
      <h3>! permission required</h3>
      <p id="perm-tool"></p>
      <p id="perm-risk"></p>
      <pre id="perm-preview"></pre>
      <button onclick="approvePermission(true)">[y] yes</button>
      <button onclick="approvePermission(false)">[n] no</button>
    </div>
  </div>

  <div id="input-bar">
    <input type="text" id="user-input" placeholder="输入消息..." />
    <button onclick="sendMessage()">发送</button>
  </div>

  <script>
    const ws = new WebSocket('ws://localhost:8080/ws');

    ws.onmessage = function(event) {
      const msg = JSON.parse(event.data);
      switch(msg.type) {
        case 'chunk':
          appendAssistantChunk(msg.data);  // 追加文字到当前 assistant 气泡
          break;
        case 'tool_call':
          appendToolCall(msg.name, msg.args, msg.risk);
          break;
        case 'tool_result':
          appendToolResult(msg.name, msg.data);
          break;
        case 'done':
          finishAssistant();
          break;
        case 'permission':
          showPermissionDialog(msg);
          break;
      }
    };

    function sendMessage() {
      const input = document.getElementById('user-input');
      const content = input.value.trim();
      if (!content) return;
      appendUserMessage(content);  // 添加用户气泡
      input.value = '';
      fetch('/api/chat', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({content: content})
      });
    }

    function approvePermission(approved) {
      hidePermissionDialog();
      fetch('/api/permission', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({approved: approved})
      });
    }
  </script>
</body>
</html>
```

### 5.3 消息气泡 DOM 结构

```javascript
// 用户消息
function appendUserMessage(content) {
  const div = document.createElement('div');
  div.className = 'message user';
  div.innerHTML = `<span class="role">▸ you</span>
                   <span class="content">${escapeHtml(content)}</span>`;
  document.getElementById('chat').appendChild(div);
  scrollToBottom();
}

// Assistant 消息（流式追加）
let currentAssistantBubble = null;
function appendAssistantChunk(chunk) {
  if (!currentAssistantBubble) {
    currentAssistantBubble = document.createElement('div');
    currentAssistantBubble.className = 'message assistant';
    currentAssistantBubble.innerHTML = '<span class="role">▸ assistant</span>';
    document.getElementById('chat').appendChild(currentAssistantBubble);
  }
  currentAssistantBubble.querySelector('.content').textContent += chunk;
  scrollToBottom();
}

function finishAssistant() {
  currentAssistantBubble = null;
}
```

---

## 六、AgentLoop 线程安全

当前 `AgentLoop::runTurn()` 是同步的，Console 模式下直接阻塞主线程。Web 模式下需要在独立线程中调用，所以要保护共享资源：

```cpp
// 需要加锁的共享资源
std::mutex session_mutex_;    // Session::addMessage() 保护
std::mutex ws_mutex_;         // WebSocket 广播列表保护

// AgentLoop 的 onEvent 回调中
void WebUi::printAssistantChunk(const std::string& chunk) {
    std::lock_guard<std::mutex> lock(ws_mutex_);
    for (auto& ws : ws_clients_) {
        ws.send(chunk);  // 线程安全
    }
}
```

**不需要锁的地方**：
- `JsonLogger::log()` — 只在 AgentLoop 线程中调用，单写者
- `ToolRegistry::find()` — 只读操作
- `PermissionManager` — 已由 `confirmPermission` 的同步机制保护

---

## 七、实现步骤

| 步骤 | 内容 | 预计时间 |
|------|------|----------|
| 1 | 从 https://github.com/yhirose/cpp-httplib 复制 `httplib.h` 到 `src/server/` | 5 min |
| 2 | 创建 `src/ui/IUi.h`，定义纯虚接口 | 15 min |
| 3 | 修改 `src/ui/Console.h`，`class Console : public IUi` | 10 min |
| 4 | 创建 `src/server/HttpServer.h/cpp`，实现基本 HTTP + WS | 90 min |
| 5 | 创建 `src/server/WebUi.h/cpp`，实现 IUi 接口 | 60 min |
| 6 | 创建 `web/index.html`，前端页面 | 90 min |
| 7 | 修改 `src/main.cpp`，加入 `--ui` 参数和 IUi 工厂 | 15 min |
| 8 | 更新 `CMakeLists.txt`，加入新源文件 | 5 min |
| 9 | 更新 `config/settings.json`，加 `ui.mode` | 2 min |
| 10 | 测试：`ai-agent.exe --ui web` → 浏览器打开 `localhost:8080` | 15 min |

**总计约 5 小时**

---

## 八、运行方式

```powershell
# Console 模式（默认，跟现在一样）
.\ai-agent.exe

# Web 模式
.\ai-agent.exe --ui web
# 浏览器打开 http://localhost:8080

# 通过配置文件切换
# config/settings.json → "ui": {"mode": "web"}
.\ai-agent.exe
```

---

## 九、注意事项

1. **cpp-httplib 许可证**：MIT，可商用
2. **WebSocket 断线重连**：前端 JS 实现，每 3 秒重试
3. **Session 隔离**：当前单用户模式，未来多用户需要 Session 池
4. **静态文件路径**：`web/` 目录放在 exe 同目录下或嵌入到 CMake 资源
5. **权限确认超时**：`confirmPermission` 设置 5 分钟超时，超时默认拒绝
6. **端口冲突**：如果 8080 被占用，通过 `config/settings.json` 或命令行参数指定
