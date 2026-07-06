# cpp-ai-agent —— C++ AI 编程智能体

`cpp-ai-agent` 是一个运行在终端中的 AI 编程智能体项目，目标是用 C++17 实现一个可演示、可扩展、可测试的简化版 Coding Agent。

项目通过大语言模型 API 理解用户输入，在需要时调用本地工具完成文件读取、文件编辑、命令执行等任务，并通过终端界面展示执行过程。

## 当前状态

当前处于 M0 阶段：项目骨架已建立，CMake 可以配置、编译并运行最小程序。

```powershell
cmake -B build -S .
cmake --build build --config Release
.\build\ai-agent.exe
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
| 构建 | CMake |
| 编译器 | MSVC / Visual Studio Build Tools |
| HTTP | cpr |
| JSON | nlohmann/json |
| TUI | FTXUI |
| 测试 | doctest 或 Catch2 |
| 存储 | JSON 文件 |

## 目录规划

```text
cpp-ai-agent/
├── CMakeLists.txt
├── README.md
├── docs/
├── src/
├── tests/
└── build/
```

## 文档

- [需求规格说明书](docs/01-需求规格说明书.md)
- [系统设计文档](docs/02-系统设计文档.md)
- [开发计划书](docs/03-开发计划书.md)

## 里程碑

| 阶段 | 目标 |
|------|------|
| M0 | CMake 项目骨架，最小程序可编译运行 |
| M1 | 接入大模型 API，实现基础对话 |
| M2 | 实现工具接口、工具注册表、文件和命令工具 |
| M3 | 实现 Agent 主循环，支持模型自主调用工具 |
| M4 | 加入权限控制、日志记录和历史回放 |
| M5 | 完善 TUI、搜索、Skill、MCP 演示和测试 |

## 说明

本项目用于课程实践和答辩演示，参考公开 AI Coding Agent 的架构思想进行独立实现，不包含任何第三方闭源产品代码。
