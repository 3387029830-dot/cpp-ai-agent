# cpp-ai-agent —— C++ AI 编程智能体

`cpp-ai-agent` 是一个运行在终端中的 AI 编程智能体项目，目标是用 C++17 实现一个可演示、可扩展、可测试的简化版 Coding Agent。

项目通过大语言模型 API 理解用户输入，在需要时调用本地工具完成文件读取、文件编辑、命令执行等任务，并通过终端界面展示执行过程。

## 当前状态

当前处于 M2 阶段：项目骨架已建立，CMake + vcpkg 可以配置、安装依赖、编译运行；M1 已完成 OpenAI 兼容格式 API 的真实对话验证，M2 已完成本地工具系统第一版。

## 快速开始

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
│   ├── config/
│   ├── core/
│   ├── llm/
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
| M3 | 实现 Agent 主循环，支持模型自主调用工具 |
| M4 | 加入权限控制、日志记录和历史回放 |
| M5 | 完善 TUI、搜索、Skill、MCP 演示和测试 |

## 说明

本项目用于课程实践和答辩演示，参考公开 AI Coding Agent 的架构思想进行独立实现，不包含任何第三方闭源产品代码。
