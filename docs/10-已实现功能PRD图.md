# cpp-ai-agent 已实现功能 PRD 图

> 日期：2026-07-07

---

## 一、配置与启动模块

### 1.1 三级配置加载

| 项目 | 内容 |
| **需求描述** | 程序启动时从三个来源加载配置，按优先级合并：系统环境变量 > `.env` 文件 > `config/settings.json` |
| **输入** | 系统环境变量、`.env` 文件（KEY=VALUE 格式，支持 `#` 注释、引号包裹）、`config/settings.json`（JSON 格式） |
| **输出** | `AppConfig` 结构体，包含 llm（baseUrl/apiKey/model/proxyUrl）、maxIterations、permissionMode、workspaceRoot、historyDir |
| **验收标准** | 1) 环境变量优先于 .env；2) .env 优先于 settings.json；3) 多路径自动搜索 .env（项目根目录、config 目录、上级目录）；4) API Key 不写入日志 |
| **实现位置** | `src/config/AppConfig.cpp:144-187` |

### 1.2 模型名称自动纠错

| 项目 | 内容 |
| **需求描述** | 自动修正常见拼写错误，如 `gpt-5.4.-mini` → `gpt-5.4-mini` |
| **输入** | OPENAI_MODEL 环境变量或 .env 中的模型名称字符串 |
| **输出** | 纠正后的模型名称 |
| **验收标准** | 1) 已知笔误自动修正；2) 正确名称原样返回；3) 不影响 API 调用 |
| **实现位置** | `src/config/AppConfig.cpp:135-140` |

### 1.3 配置诊断（/doctor）

| 项目 | 内容 |
| **需求描述** | 提供 `/doctor` 命令诊断当前配置是否正确 |
| **输入** | AppConfig 完整配置 |
| **输出** | 6 项诊断结果，每项标记 `[ok]` 或 `[warn]` |
| **验收标准** | 1) 检测 BASE_URL 是否包含 `/chat/completions` 错误后缀；2) 检测 LinkAPI URL 是否缺少 `/v1`；3) 检测 MODEL 是否匹配 Provider；4) 检测 API Key 是否为占位符或空值；5) 检测 VCPKG_ROOT 是否配置且 `vcpkg.cmake` 存在；6) 检测 workspace_root 路径是否存在 |
| **实现位置** | `src/diagnostics/Diagnostics.cpp:55-106` |

### 1.4 /init-env Provider 模板

| 项目 | 内容 |
| **需求描述** | 支持按 provider 生成 `.env` 模板文件 |
| **输入** | provider 参数（deepseek 或 linkapi） |
| **输出** | 项目根目录 `.env` 文件（如已存在则不覆盖） |
| **验收标准** | 1) deepseek 模板包含正确的 BASE_URL 和 model；2) linkapi 模板包含正确的 BASE_URL 和 model；3) 已存在时提示不覆盖；4) 生成后提示用户编辑 API Key |
| **实现位置** | `src/main.cpp:146-189` |

---

## 二、Console UI 模块

### 2.1 颜色支持检测

| 项目 | 内容 |
| **需求描述** | 启动时检测终端是否支持 ANSI 颜色输出 |
| **输入** | NO_COLOR 环境变量、TERM 环境变量、stdout 是否是 tty、Windows Console 虚拟终端支持 |
| **输出** | 布尔值，控制全局颜色开关 |
| **验收标准** | 1) NO_COLOR 设置时禁用颜色；2) TERM=dumb 时禁用；3) stdout 非交互（管道/重定向）时禁用；4) Windows 下需启用 ENABLE_VIRTUAL_TERMINAL_PROCESSING |
| **实现位置** | `src/ui/Console.cpp:323-353` |

### 2.2 启动横幅

| 项目 | 内容 |
| **需求描述** | 启动时打印程序名称、当前模型和 workspace 路径 |
| **输入** | 模型名称、workspace 路径字符串 |
| **输出** | 带 ANSI 样式的横幅文本 |
| **验收标准** | 1) 粗体显示 `cpp-ai-agent`；2) 灰色显示模型名和 workspace；3) 非交互模式降级为纯文本 |
| **实现位置** | `src/ui/Console.cpp:144-148` |

### 2.3 User 消息展示

| 项目 | 内容 |
| **需求描述** | 青色 `▸ you` 标签 + 缩进正文展示用户输入 |
| **输入** | 用户输入文本（UTF-8） |
| **输出** | 带 ANSI 样式的终端输出 |
| **验收标准** | 1) 青色标签；2) 每行缩进 2 空格；3) 多行文本正确处理 |
| **实现位置** | `src/ui/Console.cpp:163-167` |

### 2.4 Assistant 打字机效果

| 项目 | 内容 |
| **需求描述** | Assistant 回复以打字机效果流式输出 |
| **输入** | Assistant 回复文本（UTF-8） |
| **输出** | 分组逐字终端输出 |
| **验收标准** | 1) 按 UTF-8 字符边界分组（每组 3 字符）；2) 每组间隔 12ms；3) 最大打字机输出 1200 字符，超出直接打印；4) 非交互模式整段输出；5) bold `▸ assistant` 标签 |
| **实现位置** | `src/ui/Console.cpp:169-197` |

### 2.5 Tool Call 展示

| 项目 | 内容 |
| **需求描述** | 蓝色 `● 工具名` + 灰色风险等级 + 缩进参数展示工具调用 |
| **输入** | 工具名、参数 JSON 字符串、风险等级字符串 |
| **输出** | 带 ANSI 样式的终端输出 |
| **验收标准** | 1) 蓝色圆点 + 工具名；2) 灰色风险等级；3) 参数每行缩进 2 空格 |
| **实现位置** | `src/ui/Console.cpp:199-203` |

### 2.6 Tool Result 展示

| 项目 | 内容 |
| **需求描述** | 灰色 `└ 工具名:` + 结果摘要展示工具执行结果 |
| **输入** | 工具名、结果详情字符串 |
| **输出** | 带 ANSI 样式的终端输出 |
| **验收标准** | 灰色标签 + 详情文本，后跟换行 |
| **实现位置** | `src/ui/Console.cpp:205-207` |

### 2.7 权限确认 UI（方向键选择）

| 项目 | 内容 |
| **需求描述** | 高风险操作前弹出可选中确认界面，支持方向键切换 yes/no |
| **输入** | PermissionRequest（工具名、风险等级、参数、diff preview） |
| **输出** | 布尔值（同意/拒绝） |
| **验收标准** | 1) 黄色 `! permission required` 标签；2) 展示 tool/risk/args/preview 信息；3) 反色高亮当前选项（`[y] yes` / `[n] no`）；4) ↑↓ 或 ←→ 方向键切换选项；5) y/n/Enter 确认选择；6) Esc 拒绝；7) 非交互模式降级为 `[y/N]>` 文本输入；8) diff preview 以灰色缩进展示 |
| **实现位置** | `src/ui/Console.cpp:227-288` |

### 2.8 /ui 控制台概览

| 项目 | 内容 |
| **需求描述** | 展示 TUI 布局概览：Conversation/Tools/Status 区域说明 |
| **输入** | 模型名、BASE_URL、workspace、history 目录 |
| **输出** | 带 ANSI 样式的概览文本 |
| **验收标准** | 1) Conversation 区标注 user/assistant 流式显示；2) Tools 区列出 read_file/write_file/edit_file/run_command 及风险等级；3) Status 区显示 API 地址和 history 路径 |
| **实现位置** | `src/ui/Console.cpp:298-313` |

### 2.9 Warning / Error 展示

| 项目 | 内容 |
| **需求描述** | 异常情况以黄色 `!` 警告或红色 `error>` 展示 |
| **输入** | 消息文本 |
| **输出** | 带 ANSI 样式的终端输出 |
| **验收标准** | 1) Warning 黄色前缀 `!` + 消息；2) Error 红色 `error>` + 消息 |
| **实现位置** | `src/ui/Console.cpp:290-296` |

---

## 三、LLM 模块

### 3.1 ILlmClient 接口

| 项目 | 内容 |
| **需求描述** | 定义 LLM 客户端统一接口，支持注入 mock 做单元测试 |
| **输入** | `chat(messages)` 或 `chat(messages, toolsSpec)` |
| **输出** | `Message` 对象（Role::Assistant） |
| **验收标准** | 1) 纯虚接口，两种重载；2) AgentLoop 依赖接口而非具体类；3) 测试可注入 mock 实现 |
| **实现位置** | `src/llm/LlmClient.h:19-28` |

### 3.2 LlmClient HTTP 调用

| 项目 | 内容 |
| **需求描述** | 发送 OpenAI 兼容格式的 chat completions 请求 |
| **输入** | messages 列表 + 可选 toolsSpec |
| **输出** | assistant Message（含 content 和可能的 toolCalls） |
| **验收标准** | 1) POST `{baseUrl}/chat/completions`；2) 请求体含 model/messages/tools/tool_choice 字段；3) tool_calls 正确序列化（id/type:function/function:{name,arguments}）；4) tool_call_id 正确附加到 tool 消息；5) 错误时抛出异常并脱敏 API Key（`sk-***REDACTED***`）；6) Windows Schannel 禁用证书吊销检查；7) 支持 HTTP 代理 |
| **实现位置** | `src/llm/LlmClient.cpp:66-133` |

### 3.3 LlmParsing 解析

| 项目 | 内容 |
| **需求描述** | 从 API 响应中解析 assistant 消息和 tool_calls |
| **输入** | API 返回的 choices[0].message JSON 对象 |
| **输出** | Message（role=Assistant, content, toolCalls[id,name,arguments]） |
| **验收标准** | 1) content 字段缺失时返回空字符串；2) tool_calls 数组为空时不崩溃；3) tool_calls 中每个条目提取 id/function.name/function.arguments；4) name 为空的条目跳过；5) 可独立单元测试 |
| **实现位置** | `src/llm/LlmParsing.cpp:35-47` |

---

## 四、Agent 主循环模块

### 4.1 AgentLoop 核心循环

| 项目 | 内容 |
| **需求描述** | 实现 LLM ↔ 工具调用的完整闭环 |
| **输入** | Session（含用户消息和 system prompt） |
| **输出** | assistant 最终回复 Message |
| **验收标准** | 1) 通过 ContextManager 构建上下文窗口传给 LLM；2) LLM 返回无 tool_calls 则直接返回回复；3) 有 tool_calls 则逐条执行（ToolRegistry 查找 → PermissionManager 审批 → 执行）；4) 结果以 Role::Tool 消息回填 Session；5) 继续调用 LLM 直至无工具调用或达到最大轮次（默认 10）；6) 超轮次返回警告消息；7) 通过 AgentEventCallback 推送事件给 Console |
| **实现位置** | `src/agent/AgentLoop.cpp:40-97` |

### 4.2 工具执行子流程

| 项目 | 内容 |
| **需求描述** | 单个 tool_call 的执行：查工具 → 预览 → 审批 → 执行 |
| **输入** | ToolCall（id, name, arguments） |
| **输出** | tool Message（role=Tool, toolCallId, content） |
| **验收标准** | 1) 工具未找到返回 "Tool not found"；2) Safe 工具跳过 preview；3) Write/Dangerous 工具先生成 preview 再审批；4) 审批拒绝返回 "not approved" 消息；5) 执行异常返回 "Tool execution error" 消息；6) JSON 解析失败有异常处理 |
| **实现位置** | `src/agent/AgentLoop.cpp:106-136` |

### 4.3 ContextManager 上下文窗口

| 项目 | 内容 |
| **需求描述** | 限制发送给 LLM 的消息数量，防止超出 token 限制 |
| **输入** | 完整消息列表、最大消息数（默认 40） |
| **输出** | 截断后的消息列表 |
| **验收标准** | 1) 消息数 ≤ 上限时全量返回；2) 超上限时保留首条 system prompt；3) 取尾部最新 N 条消息（N = 上限 - 是否保留 system）；4) 上限至少为 1 |
| **实现位置** | `src/core/ContextManager.cpp:9-24` |

---

## 五、工具系统模块

### 5.1 ITool 统一接口

| 项目 | 内容 |
| **需求描述** | 定义所有工具必须实现的统一接口 |
| **接口方法** | `name()` / `description()` / `parametersSchema()` / `risk()` / `preview(args)` / `execute(args)` |
| **验收标准** | 1) 纯虚接口易于扩展；2) preview 有默认空实现（向后兼容）；3) RiskLevel 枚举：Safe/Write/Dangerous；4) ToolResult 包含 success/output/display 三字段 |
| **实现位置** | `src/tools/ITool.h:21-31`、`src/tools/ITool.cpp:5-8` |

### 5.2 ToolRegistry 工具注册表

| 项目 | 内容 |
| **需求描述** | 统一管理所有已注册工具的注册、查找和 OpenAI tools spec 生成 |
| **输入** | shared_ptr\<ITool\> |
| **输出** | 注册成功或抛出异常 |
| **验收标准** | 1) 使用 `unordered_map<string, shared_ptr<ITool>>` 存储；2) 拒绝空指针（抛出 invalid_argument）；3) 拒绝空名称；4) 拒绝重复注册；5) `find()` 未找到返回 nullptr；6) `names()` 按字母排序返回；7) `toolsSpec()` 生成 OpenAI function-calling 格式 JSON 数组 |
| **实现位置** | `src/tools/ToolRegistry.cpp:9-66` |

### 5.3 ReadFileTool

| 项目 | 内容 |
| **需求描述** | 读取工作区内的文件内容 |
| **输入** | `{"path": "相对路径"}` |
| **输出** | ToolResult {success, content, "Read file: 绝对路径"} |
| **风险等级** | Safe（无需确认） |
| **验收标准** | 1) 路径经 PathUtils 校验（绝对路径拼接 + 规范化 + 越界检查）；2) 文件不存在抛出异常；3) 二进制模式读取；4) 路径越界时 result.success=false |
| **实现位置** | `src/tools/FileTools.cpp:55-91` |

### 5.4 WriteFileTool

| 项目 | 内容 |
| **需求描述** | 在工作区内写入文件，写入前展示 diff 预览 |
| **输入** | `{"path": "相对路径", "content": "完整内容"}` |
| **输出** | ToolResult {true, "Wrote file: 绝对路径", ...} |
| **风险等级** | Write（需确认，展示 diff） |
| **验收标准** | 1) 路径越界检查；2) 自动创建父目录；3) preview 方法生成 unified diff 风格对比（`---/+++ @@ full file @@`，新文件标记 `(new file)`，`-`旧行 `+`新行）；4) diff 截断 4000 字符 |
| **实现位置** | `src/tools/FileTools.cpp:148-199` |

### 5.5 EditFileTool

| 项目 | 内容 |
| **需求描述** | 在工作区内精确替换文件中的文本片段 |
| **输入** | `{"path": "相对路径", "old_text": "要替换的文本", "new_text": "替换文本"}` |
| **输出** | ToolResult {true, "Edited file: 绝对路径", ...} |
| **风险等级** | Write（需确认，展示 diff） |
| **验收标准** | 1) old_text 不能为空；2) old_text 必须在文件中精确匹配（`string::find`）；3) 仅替换首次匹配；4) 匹配不到抛出异常；5) preview 方法生成 replacement diff（`@@ replacement @@`，`-`旧文本 `+`新文本） |
| **实现位置** | `src/tools/FileTools.cpp:201-274` |

### 5.6 ShellTool

| 项目 | 内容 |
| **需求描述** | 执行 shell 命令并返回输出和退出码 |
| **输入** | `{"command": "命令行字符串"}` |
| **输出** | ToolResult {success(exitCode==0), stdout+stderr, "Command exited with code N"} |
| **风险等级** | Dangerous（强制确认） |
| **验收标准** | 1) 通过 `popen`/`_popen` 执行；2) `2>&1` 合并 stderr 到 stdout；3) 4KB 缓冲循环 `fgets` 读取；4) 正确获取退出码；5) 命令不能为空 |
| **实现位置** | `src/tools/ShellTool.cpp:50-89` |

### 5.7 PathUtils 路径安全

| 项目 | 内容 |
| **需求描述** | 所有文件操作的路径安全校验层 |
| **输入** | workspaceRoot 绝对路径 + 用户请求的相对路径 |
| **输出** | 规范化后的绝对路径或抛出异常 |
| **验收标准** | 1) 相对路径拼接到 workspaceRoot 再规范化；2) 绝对路径直接规范化；3) 使用 `weakly_canonical` 处理符号链接和 `..`；4) `isPathInsideWorkspace` 做前缀匹配（Windows 大小写不敏感）；5) 越界抛出 "outside the workspace" 异常 |
| **实现位置** | `src/tools/PathUtils.cpp:32-65` |

---

## 六、安全模块

### 6.1 PermissionManager 权限审批

| 项目 | 内容 |
| **需求描述** | 根据工具风险等级和权限模式决定是否允许执行 |
| **输入** | PermissionRequest {toolName, risk, arguments, preview} |
| **输出** | bool（允许/拒绝） |
| **验收标准** | 1) Safe 工具直接允许；2) ReadOnly 模式拒绝所有非 Safe 工具；3) AskEachTime 模式调用 Console 确认（含 diff preview）；4) TrustSession 模式仅拦截黑名单危险命令（黑名单命令仍需确认）；5) 非交互模式下降级为文本提示 |
| **实现位置** | `src/security/PermissionManager.cpp:23-41` |

### 6.2 危险命令黑名单

| 项目 | 内容 |
| **需求描述** | 拦截明显的破坏性命令 |
| **匹配模式** | `format` / `del /s` / `rmdir /s` / `rd /s` / `remove-item` / `rm -rf` / `git reset --hard` / `shutdown` |
| **验收标准** | 1) 大小写不敏感；2) 子串匹配；3) 命中任一模式返回 true |
| **实现位置** | `src/security/PermissionManager.cpp:66-87` |

### 6.3 三种权限模式

| 模式 | 行为 |
| `ask_each_time` | 每次高风险操作弹确认 UI（default） |
| `read_only` | 仅允许 Safe 工具，Write/Dangerous 直接拒绝 |
| `trust_session` | 当前会话全信任，仅拦截黑名单危险命令 |

---

## 七、存储模块

### 7.1 JsonLogger JSONL 日志

| 项目 | 内容 |
| **需求描述** | 以 JSONL 格式记录完整会话 |
| **输入** | type 字符串 + data JSON 对象 |
| **输出** | `logs/session-YYYYMMDD-HHMMSS.jsonl` 文件 |
| **验收标准** | 1) 自动创建 logs 目录；2) 文件名含精确到秒的时间戳；3) 每行 `{timestamp, type, data}` JSON；4) 每次 `flush()` 确保持久化；5) 记录类型：user_message/assistant_message/tool_call/tool_result/warning |
| **实现位置** | `src/storage/JsonLogger.cpp:37-58` |

### 7.2 HistoryReader 历史回放

| 项目 | 内容 |
| **需求描述** | 列出和回放历史会话 |
| **输入** | historyDir 路径 或 JSONL 文件路径 |
| **输出** | 文件列表（含修改时间和大小）或格式化的历史文本行 |
| **验收标准** | 1) `listHistoryFiles()` 过滤 `.jsonl` 文件，按文件名倒序排列；2) `replayHistoryFile()` 逐行解析 JSON → 根据 type 格式化为 `user>` / `assistant>` / `tool_call>` / `tool_result>` / `warning>` 文本；3) 长内容截断 600 字符；4) 无效 JSON 行标记为 `invalid_log_entry>` |
| **实现位置** | `src/storage/HistoryReader.cpp:67-112` |

---

## 八、MCP 模块（Model Context Protocol）

### 8.1 StdioMcpClient 子进程通信

| 项目 | 内容 |
| **需求描述** | 通过 stdio 管道与 MCP server 子进程通信，完成 JSON-RPC 2.0 协议交互 |
| **输入** | command 命令行数组 |
| **输出** | McpServerInfo（protocolVersion/name/version/instructions/tools）或 McpToolResult（isError/text/rawContent） |
| **验收标准** | 1) Windows Win32 `CreatePipe` + `CreateProcessA` 启动子进程；2) `writeLine()` 写入 stdin（JSON + `\n`）；3) `readLine()` 逐字节读取 stdout（PeekNamedPipe 轮询，10 秒超时，子进程退出时抛异常）；4) 完整握手流程：initialize → 收响应 → initialized 通知 → tools/list → 解析工具列表；5) callTool 同样完整握手流程；6) JSON-RPC 2.0 响应校验（jsonrpc/版本、id 匹配、error 检测）；7) 析构时 1 秒等待后 TerminateProcess |
| **实现位置** | `src/mcp/McpClient.cpp:82-346` |

### 8.2 JSON-RPC 请求/响应构建

| 方法 | 说明 |
| `makeInitializeRequest(id)` | 生成 initialize 请求（protocolVersion: 2025-03-26，clientInfo: cpp-ai-agent 0.1.0） |
| `makeInitializedNotification()` | 生成 initialized 通知（无 id 字段） |
| `makeToolsListRequest(id)` | 生成 tools/list 请求 |
| `makeToolsCallRequest(id, name, args)` | 生成 tools/call 请求 |
| `parseInitializeResult(response)` | 解析 initialize 响应 → McpServerInfo |
| `parseToolsListResult(response)` | 解析 tools/list 响应 → vector\<McpTool\> |
| `parseToolsCallResult(response)` | 解析 tools/call 响应 → McpToolResult（提取 text 类型 content） |

### 8.3 McpToolAdapter 工具适配器

| 项目 | 内容 |
| **需求描述** | 将远程 MCP 工具包装为 ITool，使其可以被 AgentLoop 自动调用 |
| **输入** | command 命令行、exposedName（如 `mcp_echo`）、McpTool（name/description/inputSchema） |
| **输出** | 实现完整 ITool 接口的适配器 |
| **验收标准** | 1) `name()` 返回 exposedName；2) `description()` 加 `[MCP]` 前缀；3) `parametersSchema()` 透传 MCP tool 的 inputSchema；4) `risk()` 固定返回 Safe；5) `execute()` 每次启动新子进程完成完整握手+调用流程；6) MCP 调用失败时返回 false + 错误信息 |
| **实现位置** | `src/mcp/McpToolAdapter.cpp:34-69` |

### 8.4 工具名称规范化

| 项目 | 内容 |
| **需求描述** | 将 server 名和工具名规范化为合法标识符 |
| **输入** | serverName（如 "my-server"）、toolName（如 "do-thing"） |
| **输出** | 如 `mcp_my_server_do_thing` |
| **验收标准** | 1) `mcp_` 前缀；2) 仅保留字母数字下划线；3) 小写转换；4) 连续下划线合并；5) 尾部下划线移除 |
| **实现位置** | `src/mcp/McpToolAdapter.cpp:9-32` |

### 8.5 内置 MCP 测试 Server

| 项目 | 内容 |
| **需求描述** | 通过 `/mcp-test-server` 子进程模式启动内置 MCP server，提供 echo 和 project_info 两个工具 |
| **入口** | `ai-agent.exe /mcp-test-server` |
| **协议** | JSON-RPC 2.0 over stdin/stdout |
| **工具** | `echo`（回显 text 参数）、`project_info`（返回项目基本信息） |
| **验收标准** | 1) 支持 initialize / tools/list / tools/call 三个 method；2) 未知 method 返回 -32601 错误；3) 无 id 的消息直接跳过 |
| **实现位置** | `src/main.cpp:353-444` |

### 8.6 MCP 演示命令

| 命令 | 功能 |
| `/mcp-demo` | 启动内置 MCP server → 完成 stdio 握手 → 列出所有工具 |
| `/mcp-call-demo` | 调用内置 server 的 echo("hello from cpp-ai-agent") + project_info 并展示结果 |
| `/mcp-connect <command> [args...]` | 连接任意外部 stdio MCP server → 握手 → 列出工具 |
| `/mcp` | 显示三个 MCP 子命令用法 |

### 8.7 MCP 工具自动注册

| 项目 | 内容 |
| **需求描述** | 启动时自动注册内置 MCP 工具和 `config/mcp_servers.json` 中配置的外部 MCP server 工具 |
| **配置格式** | `{"servers": [{"name", "command", "args": [], "enabled": true}]}` |
| **验收标准** | 1) enabled=false 的 server 跳过；2) name/command 缺失时警告跳过；3) 连接失败时警告跳过（不阻止启动）；4) 成功注册后打印 "registered N tools from <server>" |
| **实现位置** | `src/main.cpp:287-351` |

---

## 九、核心数据模型

### 9.1 Message 消息模型

| 字段 | 类型 | 说明 |------|
| role | Role 枚举 | System / User / Assistant / Tool |
| content | string | 消息正文 |
| toolCalls | vector\<ToolCall\> | Assistant 消息可携带的工具调用列表 |
| toolCallId | string | Tool 消息关联的工具调用 ID |
| timestamp | int64_t | 消息时间戳 |

### 9.2 ToolCall 工具调用

| 字段 | 类型 | 说明 |------|
| id | string | API 返回的工具调用 ID |
| name | string | 工具名称 |
| arguments | string | JSON 字符串参数 |

### 9.3 Session 会话

| 方法 | 说明 |
| 构造 | 传入会话 ID |
| addMessage(Message) | 追加消息到历史列表 |
| messages() | 获取完整消息列表 |
| id() | 获取会话 ID |

---

## 十、命令行入口总览

| 命令 | 功能 | 模式 |------|
| （无参数） | 进入 Agent 对话主循环 | 交互 |
| `/exit` | 退出程序 | 交互 |
| `/history` | 列出日志文件 | 命令 |
| `/replay <path>` | 回放日志文件 | 命令 |
| `/config` | 显示配置摘要（Key 仅显示 set/missing） | 命令 |
| `/doctor` | 运行 6 项本地配置诊断 | 命令 |
| `/demo` | 打印演示脚本指南 | 命令 |
| `/ui` | 打印 TUI 概览 | 命令 |
| `/search` | 打印 Web 搜索占位说明 | 命令 |
| `/skills` | 列出配置的 Skill | 命令 |
| `/init-env <provider>` | 生成 .env 模板 | 命令 |
| `/mcp-demo` | MCP 握手演示 | 命令 |
| `/mcp-call-demo` | MCP 工具调用演示 | 命令 |
| `/mcp-connect <cmd> [args]` | 连接外部 MCP server | 命令 |
| `/mcp-test-server` | 启动内置 MCP server（子进程模式） | 命令 |
| `/mcp` | 显示 MCP 子命令帮助 | 命令 |

---

## 十一、关键设计约束

| 约束 | 说明 |
| API Key 安全 | 不硬编码、不进仓库、日志脱敏（`sk-***REDACTED***`）、/config 只显示 set/missing |
| 路径安全 | 所有文件操作经 PathUtils 越界检查，隔离在工作区内 |
| 权限纵深 | 工具风险分级 + PermissionManager 统一审批 + 危险命令黑名单 |
| 可测试性 | ILlmClient 接口支持 mock、LlmParsing 独立可测、工具可单独测试 |
| 跨平台 | Windows 优先，非 Windows 路径下有条件编译降级（MCP stdio Windows only） |
| 降级策略 | 非交互 stdout 时打字机关闭、颜色关闭、权限确认降级为文本输入 |

---

## 十二、已实现叶子节点统计

| 模块 | 叶子节点数 |
|------|-----------|
| 配置与启动 | 4 |
| Console UI | 9 |
| LLM | 3 |
| Agent 主循环 | 3 |
| 工具系统 | 7 |
| 安全模块 | 3 |
| 存储模块 | 2 |
| MCP 模块 | 7 |
| 数据模型 | 3 |
| 命令行入口 | 15 |
| **合计** | **56** |

---

> 本文档与 `docs/09-项目思维导图.md` 叶子节点一一对应。
> 未实现功能（Web 搜索、MCP Resources/Prompts/SSE Transport、Anthropic API、命令白名单、日志脱敏增强、FTXUI 全屏 TUI）将在后续 PRD 中展开。
