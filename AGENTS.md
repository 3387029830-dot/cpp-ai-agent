# cpp-ai-agent 项目规范

## 项目概述

基于 C++17 的终端 AI 编程助手。通过 OpenAI 兼容 API 调用大模型，自动操作本地工具完成编码任务。

## 构建与运行

```bash
# 配置（只需执行一次）
cmake --preset msvc-vcpkg-debug

# 编译
cmake --build build/msvc-vcpkg-debug

# 运行
.\build\msvc-vcpkg-debug\ai-agent.exe

# 运行测试
.\build\msvc-vcpkg-debug\ai-agent-tests.exe
```

## 编码规范

### C++ 标准
- **严格使用 C++17**，不允许 C++20/23 特性（如 `std::format`、concepts、ranges）
- 不启用编译器扩展（`CMAKE_CXX_EXTENSIONS OFF`）
- MSVC 编译选项：`/W4 /permissive- /utf-8`

### 命名约定
- 类名：`PascalCase`（如 `AgentLoop`、`LlmClient`、`ReadFileTool`）
- 函数/方法：`camelCase`（如 `runTurn()`、`buildWindow()`、`registerTool()`）
- 成员变量：`snake_case_`（尾部下划线，如 `maxMessages_`、`workspaceRoot_`）
- 命名空间：`snake_case`（`cpp_ai_agent::tools`、`cpp_ai_agent::security`）

### 头文件
- **只用 `#pragma once`**，不使用传统的 `#ifndef` / `#define` include guard
- include 顺序：自身头文件 → 项目内头文件 → 第三方库 → 标准库
- 项目内 include 使用 `"module/File.h"` 格式（如 `"tools/ITool.h"`），因为 `src/` 被设为 include 路径

### 代码风格
- 缩进使用 4 个空格，不使用 Tab
- 尽量使用 `const` 修饰不修改的方法和参数
- 优先在构造函数中注入依赖（依赖注入模式）
- 接口使用纯虚类（如 `ILlmClient`、`ITool`）

## 模块架构与依赖规则

```
src/
├── main.cpp          # 入口 + 命令行解析 + 组装所有组件
├── agent/            # AgentLoop：核心 思考→行动→观察 循环
├── core/             # 基础类型：Message、Session、ContextManager
├── llm/              # 大模型通信：ILlmClient 接口 + LlmClient 实现
├── tools/            # 工具插件系统：ITool 接口 + ToolRegistry + 6 个内置工具
├── mcp/              # MCP 协议支持：外部工具服务器接入
├── security/         # 权限管理：Safe/Write/Dangerous 三级风险控制
├── skills/           # 技能预设：Skill 数据结构 + SkillCatalog
├── storage/          # 日志与历史：JsonLogger + HistoryReader
├── ui/               # 终端界面：Console（ANSI 颜色 + 打字机效果）
├── config/           # 配置加载：AppConfig 从 JSON + 环境变量读取
└── diagnostics/      # 诊断工具：健康检查 + 配置展示
```

### 依赖规则（严格遵守）
1. **`core/` 不能依赖任何其他业务模块**，它是基础层
2. **`tools/` 只能依赖 `core/`**，不能依赖 `agent/`、`llm/`、`ui/`
3. **`llm/` 只依赖 `core/`**，不能依赖其他业务模块
4. **`security/` 只依赖 `core/` 和 `tools/`**（需要 `RiskLevel`）
5. **`agent/` 可以依赖所有模块**，它是编排层
6. 新增模块时，先想清楚它应该依赖谁、被谁依赖

## 新增工具规范

当你需要新增一个工具时，遵循以下步骤：

1. 在 `src/tools/` 下创建 `XxxTool.h` 和 `XxxTool.cpp`
2. 继承 `ITool` 接口，实现 6 个必需方法：`name()`、`description()`、`parametersSchema()`、`risk()`、`preview()`（可选）、`execute()`
3. 如果涉及文件操作，接收 `workspaceRoot` 构造函数参数，用 `PathUtils::resolveWorkspacePath()` 和 `isPathInsideWorkspace()` 做路径沙箱
4. 在 `main.cpp` 中注册：`tools.registerTool(std::make_shared<XxxTool>(workspaceRoot))`
5. 根据风险等级确认是否需要更新 Skill 白名单

### 风险等级指南
- `RiskLevel::Safe`：只读操作（读文件、列目录、搜索）
- `RiskLevel::Write`：修改文件但不执行外部程序（写文件、编辑文件）
- `RiskLevel::Dangerous`：执行外部命令（shell 命令）

## 测试规范

- 测试文件放在 `tests/` 目录，命名格式 `xxx_tests.cpp`
- 使用 `doctest` 框架
- 测试用例命名：`TEST_CASE("模块名: 测试描述")`
- 每个公开函数至少覆盖正常路径和边界情况

## 配置文件

- `config/settings.json`：LLM 连接参数、权限模式、工作区路径等
- `config/skills.json`：技能预设（system prompt + 工具白名单）
- `config/mcp_servers.json`：外部 MCP 工具服务器配置
- **环境变量优先级高于 JSON 配置文件**
- **不要删除配置文件中的已有字段，只能新增或修改值**

## 对话行为约定

- 修改代码后自动执行编译验证，编译失败则分析错误并修复
- 新增文件后自动更新 CMakeLists.txt（如果是 .cpp 文件）
- 文件操作前先 `read_file` 确认内容，不要凭记忆修改
- 不要修改 `vcpkg.json`，除非明确要求
- 不要引入新的第三方依赖，除非明确讨论并同意
