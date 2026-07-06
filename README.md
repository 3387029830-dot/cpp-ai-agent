# cpp-ai-agent —— C++ AI 编程智能体

`cpp-ai-agent` 是一个运行在终端中的 AI 编程智能体项目，目标是用 C++17 实现一个可演示、可扩展、可测试的简化版 Coding Agent。

项目通过大语言模型 API 理解用户输入，在需要时调用本地工具完成文件读取、文件编辑、命令执行等任务，并通过终端界面展示执行过程。

## 当前状态

当前处于 M4 阶段：项目骨架已建立，CMake + vcpkg 可以配置、安装依赖、编译运行；M1 已完成 OpenAI 兼容格式 API 的真实对话验证，M2 已完成本地工具系统第一版，M3 已接入 Agent 主循环，M4 已完成权限确认、JSON 日志和历史回放第一版。

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
```

如果使用 DeepSeek，可以改成：

```text
OPENAI_API_KEY=你的 DeepSeek API Key
OPENAI_BASE_URL=https://api.deepseek.com
OPENAI_MODEL=deepseek-chat
OPENAI_PROXY_URL=
```

也可以直接复制样例：

```powershell
copy examples\env.linkapi.example .env
copy examples\env.deepseek.example .env
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

如果 `cmake --preset` 提示找不到编译器，请在 “x64 Native Tools Command Prompt for VS” 里运行同样命令，或先执行本机 Visual Studio 的 `VsDevCmd.bat`。

查看和回放历史：

```powershell
.\build\msvc-vcpkg-debug\ai-agent.exe /history
.\build\msvc-vcpkg-debug\ai-agent.exe /replay logs\session-20260706-154848.jsonl
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

## 核心目标

- 建立完整 Agent 主循环：输入、模型决策、工具调用、结果回填、多轮执行。
- 提供插件式工具系统，支持文件工具、命令工具、搜索工具等扩展。
- 接入 OpenAI 兼容格式的大模型 API。
- 加入权限确认，避免模型直接执行高风险操作。
- 记录执行日志，支持历史回放和答辩演示。
- 使用 TUI 展示对话、工具调用、状态和结果。

## 技术栈

| 类别 | 选型 |
|------|------|
| 语言 | C++17 |
| 构建 | CMake + CMakePresets |
| 依赖管理 | vcpkg manifest |
| 编译器 | MSVC / Visual Studio Build Tools |
| HTTP | cpr |
| JSON | nlohmann/json |
| TUI | FTXUI |
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
| `.clang-format` | C++ 代码格式化规则 |
| `config/` | 程序运行配置 |
| `docs/` | 项目文档 |
| `src/` | C++ 源代码 |
| `src/agent/` | Agent 主循环，负责模型工具调用和结果回填 |
| `src/security/` | 权限确认和危险命令拦截 |
| `src/storage/` | JSONL 会话日志 |
| `tests/` | 单元测试代码 |
| `build/` | 构建产物目录，不提交到 Git |

根目录下的配置文件看起来比较多，但它们大多是 CMake、vcpkg、Git、格式化工具默认会读取的位置，因此保留在根目录更符合 C++ 工程习惯。

## 文档

- [需求规格说明书](docs/01-需求规格说明书.md)
- [系统设计文档](docs/02-系统设计文档.md)
- [开发计划书](docs/03-开发计划书.md)

## 里程碑

| 阶段 | 目标 |
|------|------|
| M0 | CMake 项目骨架，最小程序可编译运行 |
| M0.5 | vcpkg 依赖管理、配置文件、测试目录、格式化配置 |
| M1 | 接入大模型 API，实现基础对话 |
| M2 | 实现工具接口、工具注册表、文件和命令工具 |
| M3 | 实现 Agent 主循环，支持模型自主调用 `read_file` 并回填结果 |
| M4 | 加入权限控制、日志记录和历史回放 |
| M5 | 完善 TUI、搜索、Skill、MCP 演示和测试 |

## 说明

本项目用于课程实践和答辩演示，参考公开 AI Coding Agent 的架构思想进行独立实现，不包含任何第三方闭源产品代码。
