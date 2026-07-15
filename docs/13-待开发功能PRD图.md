# cpp-ai-agent 待开发功能 PRD 图

> 对应 `docs/09-项目思维导图.md` 「后续扩展」分支下的未实现叶子节点  
> 日期：2026-07-07

---

## 一、MCP Resources

### 1.1 需求背景

当前 MCP 模块仅实现 `tools/list` 和 `tools/call`。MCP 协议还定义了 `resources/list` 和 `resources/read` 方法，允许 server 暴露可读取的资源（如文件内容、数据库记录、配置片段等）。

### 1.2 功能需求

| 编号 | 功能 | 优先级 | 说明 |
| R1 | `resources/list` 请求/响应 | P1 | StdioMcpClient 新增 `listResources()` 方法，发送 JSON-RPC `resources/list` 请求，解析响应中的资源列表 |
| R2 | `resources/read` 请求/响应 | P1 | StdioMcpClient 新增 `readResource(uri)` 方法，发送 JSON-RPC `resources/read` 请求，返回资源内容 |
| R3 | McpServerInfo 扩展 | P1 | 在 `McpServerInfo` 中新增 `resources` 字段，存储 `discoverTools()` 后获取的资源列表 |
| R4 | 内置 MCP Server 扩展 | P2 | 在 `/mcp-test-server` 中新增示例 resource（如暴露 `README.md` 内容），供演示验证 |
| R5 | `/mcp-resources` 命令 | P2 | 新增 CLI 命令，连接指定 server 并列出所有资源 |
| R6 | ResourceToolAdapter | P2 | 将 MCP resource 包装为 `ITool`（如 `mcp_read_resource`），使 Agent 可通过工具调用读取资源 |

### 1.3 数据结构

```cpp
struct McpResource {
    std::string uri;         // 资源唯一标识
    std::string name;        // 人类可读名称
    std::string description; // 描述
    std::string mimeType;    // MIME 类型（可选）
};

struct McpResourceContent {
    std::string uri;
    std::string mimeType;
    std::string text;        // 文本内容
    std::string blob;        // base64 编码的二进制内容
};
```

### 1.4 验收标准

1. `StdioMcpClient::listResources()` 可正确发送 `resources/list` 请求并解析响应
2. `StdioMcpClient::readResource("file:///README.md")` 可正确发送 `resources/read` 请求并返回内容
3. 内置 MCP server 新增至少 1 个 resource（如 `project_info` resource）
4. `/mcp-resources` 命令可连接外部 server 并列出资源
5. 解析错误时抛出明确异常

### 1.5 涉及文件

| 文件 | 改动类型 |
|------|----------|
| `src/mcp/McpClient.h` | 新增数据结构 + 方法声明 |
| `src/mcp/McpClient.cpp` | 新增 JSON-RPC 请求构建 + 响应解析 + StdioMcpClient 方法实现 |
| `src/main.cpp` | 新增 `/mcp-resources` 命令 + 内置 server resource 支持 |
| `tests/mcp_tests.cpp` | 新增 resources 相关测试用例 |

### 1.6 预估工作量

**2-3 人天**

---

## 二、MCP Prompts

### 2.1 需求背景

MCP 协议定义了 `prompts/list` 和 `prompts/get` 方法，允许 server 暴露预定义的提示模板，用户或 Agent 可通过参数填充模板并触发推理。

### 2.2 功能需求

| 编号 | 功能 | 优先级 | 说明 |
| P1 | `prompts/list` 请求/响应 | P1 | StdioMcpClient 新增 `listPrompts()` 方法，发送 JSON-RPC `prompts/list` 请求，返回提示列表 |
| P2 | `prompts/get` 请求/响应 | P1 | StdioMcpClient 新增 `getPrompt(name, arguments)` 方法，返回填充后的提示消息列表 |
| P3 | McpServerInfo 扩展 | P1 | 在 `McpServerInfo` 中新增 `prompts` 字段 |
| P4 | `/mcp-prompts` 命令 | P2 | 新增 CLI 命令，列出指定 server 的所有提示模板 |

### 2.3 数据结构

```cpp
struct McpPromptArgument {
    std::string name;
    std::string description;
    bool required = false;
};

struct McpPrompt {
    std::string name;
    std::string description;
    std::vector<McpPromptArgument> arguments;
};

struct McpPromptMessage {
    std::string role;     // "user" | "assistant"
    std::string content;  // 消息文本（可能已含模板填充）
};

struct McpPromptResult {
    std::string description;
    std::vector<McpPromptMessage> messages;
};
```

### 2.4 验收标准

1. `StdioMcpClient::listPrompts()` 可正确发送 `prompts/list` 请求并解析响应
2. `StdioMcpClient::getPrompt("code_review", {{"language", "cpp"}})` 可正确发送 `prompts/get` 请求并返回填充后的消息
3. 内置 MCP server 新增 1 个示例 prompt（如 `summarize_project`）
4. `/mcp-prompts` 命令可用
5. Agent 可调用 prompt 模板（将返回的消息注入 Session 上下文）

### 2.5 涉及文件

| 文件 | 改动类型 |
|------|----------|
| `src/mcp/McpClient.h` | 新增数据结构 + 方法声明 |
| `src/mcp/McpClient.cpp` | 新增 JSON-RPC 请求构建 + 响应解析 + StdioMcpClient 方法实现 |
| `src/main.cpp` | 新增 `/mcp-prompts` 命令 + 内置 server prompt 支持 |
| `tests/mcp_tests.cpp` | 新增 prompts 相关测试用例 |

### 2.6 预估工作量

**2-3 人天**

---

## 三、SSE / HTTP Transport

### 3.1 需求背景

当前 `StdioMcpClient` 仅支持 Windows 下 stdio 子进程通信。MCP 规范还定义了 SSE（Server-Sent Events）和 HTTP transport，用于连接远程 MCP server（如云端 AI 服务）。需要新增 transport 层抽象，支持跨平台。

### 3.2 功能需求

| 编号 | 功能 | 优先级 | 说明 |
| T1 | IMcpTransport 接口 | P1 | 抽象 transport 接口：`send(request)` / `receive()` / `close()` |
| T2 | StdioTransport | P1 | 将现有 Win32 子进程逻辑封装为 StdioTransport 实现 |
| T3 | SseTransport | P1 | 基于 cpr 的 SSE 客户端：HTTP POST 发送 JSON-RPC 请求 → 解析 SSE 事件流 → 提取 JSON-RPC 响应；支持自动重连 |
| T4 | StreamableHttpTransport | P2 | 基于 cpr 的 HTTP transport：每次 POST 独立请求/响应，无流式连接 |
| T5 | StdioMcpClient 重构 | P1 | StdioMcpClient 通过 transport 接口通信，不再直接操作 pipe |
| T6 | RemoteMcpClient | P1 | 新增类，接收 URL + transport 类型，连接远程 MCP server |
| T7 | Linux/macOS StdioTransport | P1 | 使用 `posix_spawn` + `pipe` + `poll` 实现跨平台 stdio transport |
| T8 | `/mcp-connect` 扩展 | P2 | 支持 `--url <url>` 参数连接远程 server |

### 3.3 架构

```text
IMcpTransport (抽象接口)
├── StdioTransport      (子进程 stdin/stdout)
├── SseTransport        (HTTP POST + SSE 事件流)
└── StreamableHttpTransport (逐次 HTTP POST)

StdioMcpClient       (组合 transport)
RemoteMcpClient      (组合 transport + URL)
```

### 3.4 验收标准

1. `IMcpTransport` 接口定义清晰的 send/receive/close 语义
2. `StdioTransport` 在 Windows 和 Linux 下工作正常
3. `SseTransport` 可连接支持 SSE 的远程 MCP server 并完成握手
4. `StreamableHttpTransport` 可发送独立 HTTP 请求获取响应
5. `StdioMcpClient` 和 `RemoteMcpClient` 共用 transport 接口
6. 现有 MCP 演示命令（`/mcp-demo`、`/mcp-call-demo`）行为不变
7. `/mcp-connect --url http://...` 可连接远程 server

### 3.5 涉及文件

| 文件 | 改动类型 |
|------|----------|
| `src/mcp/ITransport.h` | 新增 - transport 抽象接口 |
| `src/mcp/StdioTransport.h/cpp` | 新增 - stdio 实现（跨平台） |
| `src/mcp/SseTransport.h/cpp` | 新增 - SSE 实现 |
| `src/mcp/HttpTransport.h/cpp` | 新增 - HTTP 实现 |
| `src/mcp/McpClient.h/cpp` | 重构 - 注入 transport |
| `src/mcp/RemoteMcpClient.h/cpp` | 新增 - 远程 MCP 客户端 |
| `src/main.cpp` | 修改 `/mcp-connect` 支持 URL 参数 |
| `CMakeLists.txt` | 新增源文件 |
| `tests/mcp_tests.cpp` | 新增 transport 相关测试 |

### 3.6 预估工作量

**3-5 人天**

---

## 四、Anthropic-compatible API

### 4.1 需求背景

当前仅支持 OpenAI 兼容 API 格式。Anthropic 的 Messages API 使用不同的消息结构（不含 system role，改用独立 system 参数）、工具调用格式（`tool_use` content block）和流式协议（SSE）。需要通过 `ILlmClient` 接口新增实现，使 Agent 可切换后端。

### 4.2 功能需求

| 编号 | 功能 | 优先级 | 说明 |
| A1 | AnthropicConfig | P1 | 新增配置结构体：`apiKey` / `baseUrl`(默认 `https://api.anthropic.com`) / `model` / `maxTokens` / `anthropicVersion`(默认 `2023-06-01`) |
| A2 | AnthropicClient | P1 | 实现 `ILlmClient` 接口，适配 Anthropic Messages API |
| A3 | 消息格式转换 | P1 | 将内部 `Message` 列表转为 Anthropic 格式：System prompt 走 `system` 顶层字段；User/Assistant 走 `content` 数组（`text` block）；Tool 消息走 `tool_result` content block；Tool calls 走 `tool_use` content block |
| A4 | Tool Schema 转换 | P1 | 将 OpenAI function-calling 格式的 `tools` 定义转为 Anthropic `tools` 格式（`name`/`description`/`input_schema`） |
| A5 | 响应解析 | P1 | 解析 Anthropic 响应：`text` block → content；`tool_use` block → ToolCall 列表；`stop_reason: "tool_use"` → 需要工具调用 |
| A6 | API Key Header | P1 | 使用 `x-api-key` header（非 `Authorization: Bearer`）及 `anthropic-version` header |
| A7 | `.env` 支持 | P1 | 新增 `ANTHROPIC_API_KEY` 环境变量；`/init-env anthropic` 生成模板 |
| A8 | `/doctor` 扩展 | P1 | 诊断 Anthropic 配置（Anthropic API Key、model 是否合法、BASE_URL 是否正确） |
| A9 | Provider 切换 | P1 | `settings.json` 新增 `provider` 字段（`openai` 或 `anthropic`），main.cpp 根据 provider 创建对应 Client |

### 4.3 两种 API 格式对比

```text
OpenAI 格式:
{
  "model": "gpt-4o",
  "messages": [
    {"role": "system", "content": "You are..."},
    {"role": "user", "content": "Hello"},
    {"role": "assistant", "content": null, "tool_calls": [{"id": "1", "type": "function", "function": {"name": "read_file", "arguments": "{\"path\":\"README.md\"}"}}]},
    {"role": "tool", "tool_call_id": "1", "content": "file content..."}
  ],
  "tools": [{"type": "function", "function": {"name": "read_file", "description": "...", "parameters": {...}}}]
}

Anthropic 格式:
{
  "model": "claude-sonnet-4-6",
  "system": "You are...",
  "messages": [
    {"role": "user", "content": [{"type": "text", "text": "Hello"}]},
    {"role": "assistant", "content": [{"type": "tool_use", "id": "1", "name": "read_file", "input": {"path": "README.md"}}]},
    {"role": "user", "content": [{"type": "tool_result", "tool_use_id": "1", "content": "file content..."}]}
  ],
  "tools": [{"name": "read_file", "description": "...", "input_schema": {...}}]
}
```

### 4.4 验收标准

1. `AnthropicClient::chat()` 可发送正确格式的请求并获得回复
2. 消息格式转换正确处理所有 4 种 Role（System → 顶层 system 字段、User/Assistant/Tool → content 数组）
3. Tool calls 正确序列化为 `tool_use` block 和回填 `tool_result` block
4. Tool schema 格式转换正确（`parameters` → `input_schema`，移除 `type: function` 包装）
5. `/init-env anthropic` 生成含 `ANTHROPIC_API_KEY` 的 `.env` 模板
6. `/doctor` 可诊断 Anthropic 配置
7. `settings.json` 中 `provider: "anthropic"` 可正确切换后端
8. 测试用例覆盖消息格式转换和 Tool schema 转换

### 4.5 涉及文件

| 文件 | 改动类型 |
|------|----------|
| `src/llm/AnthropicClient.h/cpp` | 新增 - Anthropic API 实现 |
| `src/llm/LlmClient.h` | 无需改动（已有 ILlmClient 接口） |
| `src/llm/LlmParsing.h/cpp` | 新增 Anthropic 响应解析函数 |
| `src/config/AppConfig.h` | 新增 provider 字段 |
| `src/config/AppConfig.cpp` | 新增 Anthropic 配置加载逻辑 |
| `src/diagnostics/Diagnostics.cpp` | 新增 Anthropic 诊断项 |
| `src/main.cpp` | 根据 provider 创建不同 Client + `/init-env anthropic` |
| `tests/llm_parsing_tests.cpp` | 新增 Anthropic 格式解析测试 |
| `tests/anthropic_tests.cpp` | 新增 - Anthropic Client 测试 |

### 4.6 预估工作量

**2-3 人天**

---

## 五、命令白名单

### 5.1 需求背景

当前安全模块仅维护危险命令黑名单。在生产环境中，更实用的模式是「默认拒绝所有命令，仅允许预定义的安全命令列表」，即白名单模式。两种模式应可共存、可切换。

### 5.2 功能需求

| 编号 | 功能 | 优先级 | 说明 |
| W1 | 白名单配置 | P1 | `settings.json` 的 `permission` 段新增 `command_allowlist` 字段，支持字符串数组（如 `["dir", "ls", "git status", "git diff", "git log", "echo", "type", "cat"]`） |
| W2 | 白名单模式 | P1 | `PermissionManager` 新增 `CommandPolicy` 枚举：`Blacklist`（当前行为）、`Whitelist`（仅允许白名单命令）、`BlacklistAndWhitelist`（白名单放行 + 黑名单拦截） |
| W3 | 命令匹配算法 | P1 | 支持前缀匹配（`git` 匹配 `git status`/`git diff`/`git log`）和精确匹配；大小写不敏感（Windows） |
| W4 | `/doctor` 扩展 | P2 | 显示当前命令策略和白名单/黑名单条目数 |
| W5 | 非白名单命令处理 | P1 | 白名单模式下未匹配的命令直接被 PermissionManager 拒绝，不弹出确认 UI |
| W6 | 内置安全列表 | P2 | 内置最小安全命令列表（`dir`/`ls`/`echo`/`type`/`cat`/`git status`/`git diff`/`git log`），即使配置缺失也有基本保护 |

### 5.3 配置格式

```json
{
  "permission": {
    "mode": "ask_each_time",
    "command_policy": "whitelist",
    "command_allowlist": [
      "dir", "ls", "echo", "type", "cat",
      "git status", "git diff", "git log", "git branch",
      "cmake", "ctest", "clang-format"
    ],
    "command_blocklist": [
      "format ", "del /s", "rm -rf", "shutdown", "git reset --hard"
    ]
  }
}
```

### 5.4 验收标准

1. `command_policy: "whitelist"` 模式下，`echo hello` 允许执行
2. 同上模式下，`del important.txt` 被拒绝（不在白名单）
3. `command_policy: "blacklist_and_whitelist"` 模式下，白名单命令仍需检查黑名单
4. 空白名单时（配置缺失），回退到内置最小安全列表
5. `git status` 前缀匹配通过，`git reset --hard` 被黑名单拦截（即使在白名单中）
6. `/doctor` 显示策略名称和条目数
7. 测试覆盖三种策略模式

### 5.5 涉及文件

| 文件 | 改动类型 |
|------|----------|
| `src/security/PermissionManager.h` | 新增 CommandPolicy 枚举 + 白名单字段 + 方法声明 |
| `src/security/PermissionManager.cpp` | 新增命令匹配算法 + 白名单逻辑 |
| `src/config/AppConfig.cpp` | 新增 command_policy 和 command_allowlist 解析 |
| `config/settings.json` | 新增配置默认值 |
| `src/diagnostics/Diagnostics.cpp` | 新增策略诊断 |
| `tests/security_tests.cpp` | 新增白名单策略测试用例 |

### 5.6 预估工作量

**1-2 人天**

---

## 六、更细粒度日志脱敏

### 6.1 需求背景

当前仅对 API Key（`sk-` 前缀）做正则脱敏。实际使用中，日志可能包含更多敏感信息：密码、token、证书、个人身份信息（PII）等。需要可配置的脱敏规则引擎。

### 6.2 功能需求

| 编号 | 功能 | 优先级 | 说明 |
| S1 | 脱敏规则配置 | P1 | `settings.json` 新增 `log_redaction` 段，支持 `rules` 数组，每条规则含 `name`、`pattern`（正则）、`replacement`（替换文本） |
| S2 | 内置规则集 | P1 | 内置规则：API Key（`sk-[^\s"'}\]<]+`）、Bearer Token（`Bearer\s+[^\s"'}\]<]+`）、环境变量值（`=[A-Za-z0-9+/=]{20,}`），可被配置覆盖 |
| S3 | 脱敏引擎 | P1 | `JsonLogger` 新增脱敏方法 `redact()`，在 `log()` 写入前对 data JSON 的所有字符串值递归应用脱敏规则 |
| S4 | 脱敏深度控制 | P2 | 配置 `max_redact_depth` 控制 JSON 嵌套脱敏深度（默认 10），防止性能问题 |
| S5 | `/doctor` 扩展 | P2 | 显示脱敏规则数量 |
| S6 | 脱敏日志 | P2 | 脱敏过程中记录被命中规则名称和次数（脱敏后写入） |

### 6.3 配置格式

```json
{
  "log_redaction": {
    "enabled": true,
    "max_redact_depth": 10,
    "rules": [
      {
        "name": "api_key",
        "pattern": "sk-[a-zA-Z0-9]{20,}",
        "replacement": "sk-***REDACTED***"
      },
      {
        "name": "bearer_token",
        "pattern": "Bearer\\s+[a-zA-Z0-9._\\-]+",
        "replacement": "Bearer ***REDACTED***"
      },
      {
        "name": "private_key",
        "pattern": "-----BEGIN [A-Z ]+ PRIVATE KEY-----[\\s\\S]*?-----END [A-Z ]+ PRIVATE KEY-----",
        "replacement": "***PRIVATE KEY REDACTED***"
      }
    ]
  }
}
```

### 6.4 验收标准

1. 内置规则在配置缺失时生效
2. 自定义规则可覆盖/扩展内置规则
3. `JsonLogger::log()` 对顶层和嵌套 JSON 字符串值均生效
4. 超过 `max_redact_depth` 的嵌套不处理（不崩溃）
5. 数字、布尔、null 值不参与脱敏（仅字符串）
6. `/doctor` 展示当前脱敏规则数量
7. 脱敏不改变 JSON 结构，仅替换字符串值
8. 测试覆盖内置规则、自定义规则、嵌套对象、数组中的敏感信息

### 6.5 涉及文件

| 文件 | 改动类型 |
|------|----------|
| `src/storage/LogRedactor.h/cpp` | 新增 - 脱敏引擎 |
| `src/storage/JsonLogger.h/cpp` | 修改 - 在 log() 中调用脱敏 |
| `src/config/AppConfig.h` | 新增 LogRedactionConfig 结构体 |
| `src/config/AppConfig.cpp` | 新增脱敏配置解析 |
| `config/settings.json` | 新增 log_redaction 默认配置 |
| `src/diagnostics/Diagnostics.cpp` | 新增脱敏规则数量诊断 |
| `tests/log_redaction_tests.cpp` | 新增 - 脱敏引擎测试 |

### 6.6 预估工作量

**1-2 人天**

---

## 七、FTXUI 全屏 TUI

### 7.1 需求背景

当前交互为同步阻塞式行式控制台（`std::getline` + `std::cout`）。虽然已有 `Console` 类的 ANSI 样式输出和打字机效果，但缺少真正的全屏 TUI 体验（多面板分屏、流式 token 实时显示、消息历史滚动、快捷键操作）。项目已依赖 FTXUI，需全面启用其组件系统。

### 7.2 功能需求

| 编号 | 功能 | 优先级 | 说明 |
| U1 | 四面板布局 | P1 | 对话历史区（上）+ 工具调用区（中）+ 状态栏（下左）+ 输入区（下），比例约 60%/20%/5%/15% |
| U2 | 对话历史面板 | P1 | 使用 `ftxui::Menu` 或 `Render` 显示消息列表：user（青色）、assistant（白色）、tool_call（蓝色缩进）、tool_result（灰色缩进）；支持上下滚动 |
| U3 | 流式 Token 实时显示 | P1 | Assistant 回复不再是打字机逐字延迟，而是 LLM 返回的每个 token 实时追加到对话面板（如 API 支持 streaming） |
| U4 | 工具调用面板 | P1 | 展示当前 tool_call 名称、参数（截断）、执行状态（pending/running/done/error）和结果摘要 |
| U5 | 状态栏 | P1 | 固定底部一行：模型名 + 本次对话工具调用次数 + 会话消息数 + 日志文件路径 |
| U6 | 输入区 | P1 | 底部输入框，支持历史命令（↑↓ 浏览）、多行粘贴、Tab 补全工具名 |
| U7 | 快捷键 | P2 | `Ctrl+C` 中断当前工具/请求；`Ctrl+L` 清屏；`Ctrl+S` 保存当前会话；`Esc` 取消权限确认 |
| U8 | 权限确认弹窗 | P1 | 覆盖层弹窗（非行式），展示 diff preview（滚动查看）+ `[Yes]` / `[No]` 按钮（←→ 选择，Enter 确认） |
| U9 | 响应式降级 | P1 | stdout 非交互时自动降级为当前行式模式（`Console` 类） |
| U10 | 颜色主题 | P2 | settings.json 新增 `theme` 配置（dark / light / high_contrast），映射 ANSI 色值 |

### 7.3 布局设计

```text
┌────────────────────────────────────────────┐
│ ▸ 对话历史                                 │
│                                            │
│ ▸ you                                      │
│   读取 README.md 并总结                      │
│                                            │
│ ▸ assistant                                │
│   我来读取 README.md 文件...                 │
│                                            │
│ ● read_file  safe                          │
│   {"path": "README.md"}                     │
│ └ read_file: # cpp-ai-agent...             │
│                                            │
│ ▸ assistant                                │
│   这个项目是一个用 C++17 实现的...            │
│                                            │
├────────────────────────────────────────────┤
│ Tools: read_file ✓  write_file —  edit —   │
├────────────────────────────────────────────┤
│ gpt-5.4-mini │ tools: 3 │ msgs: 12 │ log   │
├────────────────────────────────────────────┤
│ › 请帮我创建一个...                          │
└────────────────────────────────────────────┘
```

### 7.4 验收标准

1. 程序启动后显示四面板 FTXUI 全屏界面
2. `std::getline` 替换为 FTXUI `Input` 组件，Enter 发送消息
3. 用户消息实时出现在对话历史区
4. Assistant 回复实时流式追加到对话区（token 级别）
5. 工具调用以蓝色样式出现在对话区，结果以灰色样式追加
6. 状态栏实时更新工具调用次数和消息数
7. 权限确认以覆盖层弹窗展示，含 diff 滚动浏览
8. `Ctrl+C` 中断当前操作（LLM 请求或工具执行）
9. stdout 非交互时（管道/重定向），自动降级为行式 Console 模式
10. 退出时恢复终端原始状态

### 7.5 架构重构

```text
main.cpp
├── 非交互模式 → 当前 Console 类（不变）
└── 交互模式 → TuiApp 类（新增）
    ├── ConversationPanel   (对话历史)
    ├── ToolPanel           (工具调用状态)
    ├── StatusBar           (状态栏)
    ├── InputBar            (输入框)
    ├── PermissionDialog    (权限弹窗)
    └── EventLoop           (FTXUI 事件循环)
```

### 7.6 涉及文件

| 文件 | 改动类型 |
|------|----------|
| `src/ui/TuiApp.h/cpp` | 新增 - FTXUI 全屏应用主类 |
| `src/ui/ConversationPanel.h/cpp` | 新增 - 对话面板组件 |
| `src/ui/ToolPanel.h/cpp` | 新增 - 工具面板组件 |
| `src/ui/StatusBar.h/cpp` | 新增 - 状态栏组件 |
| `src/ui/InputBar.h/cpp` | 新增 - 输入框组件 |
| `src/ui/PermissionDialog.h/cpp` | 新增 - 权限确认弹窗组件 |
| `src/ui/Theme.h` | 新增 - 颜色主题定义 |
| `src/ui/Console.h/cpp` | 保留 - 降级模式使用 |
| `src/ui/StreamingClient.h/cpp` | 新增 - LLM streaming 支持（可选，作为增量） |
| `src/main.cpp` | 修改 - 根据交互模式选择 TuiApp 或 Console |
| `CMakeLists.txt` | 新增源文件 |
| `config/settings.json` | 新增 theme 配置 |

### 7.7 预估工作量

**3-5 人天**（不含 streaming API 改造）；若含 streaming 改造，**+1-2 人天**

---

## 工作量汇总

| 编号 | 功能 | 优先级 | 预估人天 |
|------|------|--------|----------|
| 1 | MCP Resources | P1/P2 | 2-3 |
| 2 | MCP Prompts | P1/P2 | 2-3 |
| 3 | SSE / HTTP Transport | P1 | 3-5 |
| 4 | Anthropic-compatible API | P1 | 2-3 |
| 5 | 命令白名单 | P1 | 1-2 |
| 6 | 细粒度日志脱敏 | P1 | 1-2 |
| 7 | FTXUI 全屏 TUI | P1 | 3-5 |
| **合计** | | | **14-23 人天** |

---

## 推荐开发顺序

```
第 1 步: 命令白名单         (1-2d) — 安全增强，改动最小，快速见效
第 2 步: 细粒度日志脱敏     (1-2d) — 安全增强，独立模块，低耦合
第 3 步: Anthropic API     (2-3d) — 基于 ILlmClient 接口，纯增量
第 4 步: MCP Resources     (2-3d) — MCP 协议进阶，复用现有架构
第 5 步: MCP Prompts       (2-3d) — MCP 协议进阶，与 Resources 共享模式
第 6 步: SSE/HTTP Transport (3-5d) — 需重构 transport 层，影响面大
第 7 步: FTXUI 全屏 TUI     (3-5d) — 最大改动，建议最后做
```

---

> 本文档为待开发功能 PRD，每个功能注明需求背景、数据结构、验收标准、涉及文件和预估工作量。
> 用户确认后可按推荐顺序逐步实现。
