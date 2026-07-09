# cpp-ai-agent —— C++ AI 编程智能体

`cpp-ai-agent` 是一个运行在终端中的 AI 编程智能体项目，目标是用 C++17 实现一个可演示、可扩展、可测试的简化版 Coding Agent。

项目通过大语言模型 API 理解用户输入，在需要时调用本地工具完成文件读取、文件编辑、命令执行等任务，并通过终端界面展示执行过程。

## 当前状态

当前版本：v0.1.0。项目已完成 M0-M8 演示版闭环：M0-M5 覆盖 CMake + vcpkg 构建、OpenAI 兼容 API 对话、工具调用、权限确认、JSONL 日志、历史回放、配置诊断、ANSI 增强流式控制台界面和答辩材料；M6-M8 继续补充 Web 搜索、最小 MCP 客户端和跨环境工程化稳定性。

在 v0.1.0 交付版之后，项目增强了 AgentLoop 的可测试性、终端呈现、搜索体验、MCP 集成和 Windows 构建稳定性：新增 `ContextManager` 上下文窗口、可注入的 `ILlmClient` 接口、独立 `tool_calls` 解析测试、Agent 主循环 mock LLM 自动化测试、`src/ui/Console.*` 流式控制台呈现模块、Bing RSS 优先的 Web 搜索、可由模型自动调用的内置 MCP 工具、Visual Studio generator 默认构建和 Windows 中文命令行参数修复。

## 快速开始

### 1. 准备构建环境

Windows 推荐安装：

- Visual Studio Build Tools，包含 MSVC C++ 工具链。
- CMake。
- vcpkg。

如果电脑上还没有 vcpkg，可以放到固定目录，例如：

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
setx VCPKG_ROOT C:\vcpkg
```

重新打开 PowerShell 后确认：

```powershell
echo $env:VCPKG_ROOT
```

`CMakePresets.json` 默认读取 `$env:VCPKG_ROOT`，不会绑定某一台电脑的 Visual Studio 安装路径。
如果这里没有输出路径，先按上面的命令安装 vcpkg，并重新打开 PowerShell。

如果不想设置全局环境变量，也可以复制本地 preset 样例：

```powershell
copy CMakeUserPresets.json.example CMakeUserPresets.json
notepad CMakeUserPresets.json
cmake --preset local-msvc-vcpkg-debug
cmake --build --preset local-msvc-vcpkg-debug
```

`CMakeUserPresets.json` 已被 Git 忽略，适合保存每台电脑自己的 vcpkg 路径。

### 2. 克隆和配置

克隆仓库：

```powershell
git clone https://github.com/3387029830-dot/cpp-ai-agent.git
cd cpp-ai-agent
```

复制环境变量样例并填写自己的 API Key：

```powershell
copy .env.example .env
notepad .env
```

`.env` 示例：

```text
OPENAI_API_KEY=你的 API Key
OPENAI_BASE_URL=https://api.linkapi.ai/v1
OPENAI_MODEL=gpt-5.4-mini
OPENAI_PROXY_URL=
WEB_SEARCH_PROXY_URL=
```

如果使用 DeepSeek，可以改成：

```text
OPENAI_API_KEY=你的 DeepSeek API Key
OPENAI_BASE_URL=https://api.deepseek.com
OPENAI_MODEL=deepseek-chat
OPENAI_PROXY_URL=
WEB_SEARCH_PROXY_URL=
```

也可以直接复制样例：

```powershell
copy examples\env.linkapi.example .env
copy examples\env.deepseek.example .env
```

或者让程序生成 `.env` 模板：

```powershell
.\build\msvc-vcpkg-debug\ai-agent.exe /init-env deepseek
.\build\msvc-vcpkg-debug\ai-agent.exe /init-env linkapi
```

生成后打开 `.env`，把占位 API Key 换成真实 Key。

可选：启用外部 stdio MCP server。

编辑 `config/mcp_servers.json`，把对应 server 的 `enabled` 改为 `true`。程序启动时会自动连接启用的 server，发现工具，并注册成 `mcp_<server>_<tool>` 形式供模型调用。

示例：

```json
{
  "servers": [
    {
      "name": "filesystem",
      "enabled": true,
      "command": "npx",
      "args": ["-y", "@modelcontextprotocol/server-filesystem", "."]
    }
  ]
}
```

### 3. 构建和运行

构建并运行：

```powershell
cmake --preset msvc-vcpkg-debug
cmake --build --preset msvc-vcpkg-debug
.\build\msvc-vcpkg-debug\ai-agent.exe
```

也可以不创建 `.env`，改用系统环境变量：

```powershell
$env:OPENAI_API_KEY="你的 API Key"
```

程序读取配置的优先级为：系统环境变量 > `.env` > `config/settings.json`。

默认 `msvc-vcpkg-debug` 使用 Visual Studio generator，不需要手动打开 “x64 Native Tools Command Prompt”。如果你从旧版本更新后遇到 “generator does not match” 或仍然看到 `NMake Makefiles`，删除旧构建目录后重新配置：

```powershell
Remove-Item -Recurse -Force build\msvc-vcpkg-debug
cmake --preset msvc-vcpkg-debug
cmake --build --preset msvc-vcpkg-debug
```

如果必须使用 NMake，可以改用 `nmake-vcpkg-debug` preset，但需要先进入 “x64 Native Tools Command Prompt for VS” 或执行 `vcvars64.bat`。

如果报错 `Could not find toolchain file: /scripts/buildsystems/vcpkg.cmake`，说明当前终端没有 `VCPKG_ROOT`。先设置 vcpkg 路径，再重新打开 PowerShell：

```powershell
setx VCPKG_ROOT C:\vcpkg
```

如果报错 `fatal error C1083: 无法打开包括文件: “cstdint”`，通常是旧版 NMake 构建缓存或没有加载 MSVC 环境。优先使用默认的 `msvc-vcpkg-debug` preset，并删除旧构建目录重新配置：

```powershell
Remove-Item -Recurse -Force build\msvc-vcpkg-debug
cmake --preset msvc-vcpkg-debug
cmake --build --preset msvc-vcpkg-debug
```

### 4. 一键验收流程

```powershell
cmake --preset msvc-vcpkg-debug
cmake --build --preset msvc-vcpkg-debug
cd build\msvc-vcpkg-debug
ctest -C Debug --output-on-failure
cd ..\..
.\build\msvc-vcpkg-debug\ai-agent.exe /doctor
.\build\msvc-vcpkg-debug\ai-agent.exe /demo
```

可选演示：

```powershell
.\build\msvc-vcpkg-debug\ai-agent.exe /ui
.\build\msvc-vcpkg-debug\ai-agent.exe /skills
.\build\msvc-vcpkg-debug\ai-agent.exe /search cpp-ai-agent MCP stdio
.\build\msvc-vcpkg-debug\ai-agent.exe /mcp-demo
.\build\msvc-vcpkg-debug\ai-agent.exe /mcp-call-demo
.\build\msvc-vcpkg-debug\ai-agent.exe /mcp-connect <command> [args...]
.\build\msvc-vcpkg-debug\ai-agent.exe /history
```

查看和回放历史：

```powershell
.\build\msvc-vcpkg-debug\ai-agent.exe /history
.\build\msvc-vcpkg-debug\ai-agent.exe /replay logs\session-20260706-154848.jsonl
```

查看演示脚本：

```powershell
.\build\msvc-vcpkg-debug\ai-agent.exe /demo
```

查看 M6-M8 扩展演示入口：

```powershell
.\build\msvc-vcpkg-debug\ai-agent.exe /ui
.\build\msvc-vcpkg-debug\ai-agent.exe /search cpp-ai-agent MCP stdio
.\build\msvc-vcpkg-debug\ai-agent.exe /skills
.\build\msvc-vcpkg-debug\ai-agent.exe /mcp-demo
.\build\msvc-vcpkg-debug\ai-agent.exe /mcp-call-demo
.\build\msvc-vcpkg-debug\ai-agent.exe /mcp-connect <command> [args...]
```

`/search` 使用独立的 Web 搜索代理配置，默认不走代理。搜索会优先尝试 Bing RSS，再尝试 DuckDuckGo Instant Answer；如果两个入口都不可用，会降级输出可手动打开的搜索链接。如果你的网络需要代理，可以改 `config/settings.json` 中的 `web_search.proxy_url`，或在当前 PowerShell 里覆盖：

```powershell
$env:WEB_SEARCH_PROXY_URL="http://127.0.0.1:7897"
.\build\msvc-vcpkg-debug\ai-agent.exe /search cpp-ai-agent MCP stdio
```

诊断当前配置：

```powershell
.\build\msvc-vcpkg-debug\ai-agent.exe /config
.\build\msvc-vcpkg-debug\ai-agent.exe /doctor
```

`/config` 不会显示真实 API Key，只会显示是否已设置。若 `/doctor` 显示 `OPENAI_MODEL` 仍是旧值，说明系统环境变量覆盖了 `.env`；可在当前 PowerShell 临时修正：

```powershell
$env:OPENAI_MODEL="gpt-5.4-mini"
```

如果 API 返回 `model_not_found`，优先检查模型名是否拼错。程序会自动把常见误拼 `gpt-5.4.-mini` 纠正为 `gpt-5.4-mini`，但仍建议在 `.env` 或系统环境变量中写正确值。

在主对话中可以启用 Skill，让 Agent 切换到对应工作模式：

```text
/skills
/use-skill code_review src/mcp
/use-skill cpp_debug build-log.txt
/use-skill project_summary docs
/use-skill test_writer tests
```

每个 Skill 可以配置 `allowed_tools`，启用后 AgentLoop 会阻止该 Skill 白名单之外的工具调用。

## 核心目标

- 建立完整 Agent 主循环：输入、模型决策、工具调用、结果回填、多轮执行。
- 通过 `ContextManager` 保留 system prompt 并限制发送给模型的上下文窗口。
- 提供插件式工具系统，支持文件工具、命令工具、搜索工具等扩展。
- 提供 `web_search` 安全工具和 `/search <query>` 命令，支持 Web 搜索、结构化结果整理和引用链接输出。
- 接入 OpenAI 兼容格式的大模型 API。
- 提供最小 MCP stdio 客户端，支持初始化、工具列表发现和基础工具调用；内置 MCP 工具会注册进 AgentLoop，可由模型自动调用。
- 可从 `config/mcp_servers.json` 读取外部 stdio MCP server，自动发现并注册工具。
- 支持 `/use-skill <name> [target]` 在主对话中启用 Skill，将 code review、C++ debug、项目总结、测试生成等专家提示和目标参数注入 Agent 上下文，并按 Skill 工具白名单限制工具调用。
- 提供 `list_dir` 安全工具，目录型任务可先列目录再逐个读取文件。
- 加入权限确认，避免模型直接执行高风险操作；写入和编辑文件前会显示 diff 预览再请求确认。
- 记录执行日志，支持历史回放和答辩演示。
- 使用 ANSI 增强的流式控制台展示对话、工具调用、权限确认、状态和结果。

## 技术栈

| 类别 | 选型 |
|------|------|
| 语言 | C++17 |
| 构建 | CMake + CMakePresets |
| 依赖管理 | vcpkg manifest |
| 编译器 | MSVC / Visual Studio Build Tools |
| HTTP | cpr |
| JSON | nlohmann/json |
| 终端呈现 | ANSI 流式控制台，保留 FTXUI 依赖用于兼容早期演示 |
| 测试 | doctest 或 Catch2 |
| 存储 | JSON 文件 |

## 当前项目结构

```text
cpp-ai-agent/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── README.md
├── .gitignore
├── .env.example
├── .clang-format
├── config/
│   ├── settings.json
│   └── skills.json
├── docs/
│   ├── 01-需求规格说明书.md
│   ├── 02-系统设计文档.md
│   └── 03-开发计划书.md
├── src/
│   ├── main.cpp
│   ├── agent/
│   ├── config/
│   ├── core/
│   ├── llm/
│   ├── security/
│   ├── storage/
│   └── tools/
├── tests/
│   ├── config_tests.cpp
│   └── tools_tests.cpp
└── build/
```

说明：

| 路径 | 作用 |
|------|------|
| `CMakeLists.txt` | CMake 构建入口，说明项目如何编译 |
| `CMakePresets.json` | CMake 构建预设，统一配置命令 |
| `vcpkg.json` | vcpkg 依赖清单，声明第三方库 |
| `.env.example` | 环境变量样例，不存放真实密钥 |
| `.env` | 本地真实环境变量文件，不提交到 Git |
| `config/mcp_servers.json` | 外部 stdio MCP server 配置，默认示例为禁用状态 |
| `config/skills.json` | Skill 配置，内置 `code_review`、`cpp_debug`、`project_summary`、`test_writer` |
| `.clang-format` | C++ 代码格式化规则 |
| `config/` | 程序运行配置 |
| `docs/` | 项目文档 |
| `src/` | C++ 源代码 |
| `src/agent/` | Agent 主循环，负责模型工具调用和结果回填 |
| `src/core/ContextManager.*` | 构建发送给模型的上下文窗口 |
| `src/llm/LlmParsing.*` | 解析 OpenAI-compatible `tool_calls` 响应 |
| `src/mcp/McpClient.*` | 最小 MCP stdio 客户端，负责初始化、工具列表发现和基础工具调用 |
| `src/mcp/McpToolAdapter.*` | 将 MCP tool 包装成项目内 `ITool`，注册进 AgentLoop |
| `src/security/` | 权限确认、diff 预览确认和危险命令拦截 |
| `src/storage/` | JSONL 会话日志 |
| `src/tools/WebSearchTool.*` | Web 搜索与检索结果整理工具 |
| `src/ui/Console.*` | ANSI 增强流式控制台呈现 |
| `tests/` | 单元测试代码 |
| `build/` | 构建产物目录，不提交到 Git |

根目录下的配置文件看起来比较多，但它们大多是 CMake、vcpkg、Git、格式化工具默认会读取的位置，因此保留在根目录更符合 C++ 工程习惯。

## 文档

- [需求规格说明书](docs/01-需求规格说明书.md)
- [系统设计文档](docs/02-系统设计文档.md)
- [开发计划书](docs/03-开发计划书.md)
- [演示说明](docs/04-演示说明.md)
- [答辩提纲](docs/05-答辩提纲.md)
- [测试报告](docs/06-测试报告.md)
- [项目总结](docs/07-项目总结.md)
- [版本日志](docs/08-版本日志.md)
- [项目思维导图](docs/09-项目思维导图.md)

## 里程碑

| 阶段 | 目标 |
|------|------|
| M0 | CMake 项目骨架，最小程序可编译运行 |
| M0.5 | vcpkg 依赖管理、配置文件、测试目录、格式化配置 |
| M1 | 接入大模型 API，实现基础对话 |
| M2 | 实现工具接口、工具注册表、文件和命令工具 |
| M3 | 实现 Agent 主循环，支持模型自主调用 `read_file` 并回填结果 |
| M4 | 加入权限控制、日志记录和历史回放 |
| M5 | 完善 ANSI 流式控制台、配置诊断、Skill 和答辩演示 |
| M6 | 增加 Web 搜索工具和 `/search <query>`，国内网络优先使用 Bing RSS，并提供 DuckDuckGo/Bing 兜底链接 |
| M7 | 实现最小 MCP stdio 客户端、内置 MCP 测试 server、MCP 工具调用和外部 server 工具发现 |
| M8 | 完成跨环境稳定性优化：Visual Studio generator 默认构建、NMake 备用 preset、固定 exe 输出目录、Windows 中文命令行参数修复和 GitHub/GitLab 同步流程 |

## 说明

本项目用于课程实践和答辩演示，参考公开 AI Coding Agent 的架构思想进行独立实现，不包含任何第三方闭源产品代码。
