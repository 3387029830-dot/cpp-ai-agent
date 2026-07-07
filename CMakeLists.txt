cmake_minimum_required(VERSION 3.20)

project(cpp_ai_agent
    VERSION 0.1.0
    DESCRIPTION "A terminal AI coding agent written in C++17"
    LANGUAGES CXX
)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(doctest CONFIG REQUIRED)
find_package(ftxui CONFIG REQUIRED)

add_executable(ai-agent
    src/agent/AgentLoop.cpp
    src/config/AppConfig.cpp
    src/core/ContextManager.cpp
    src/core/Message.cpp
    src/core/Session.cpp
    src/diagnostics/Diagnostics.cpp
    src/llm/LlmClient.cpp
    src/llm/LlmParsing.cpp
    src/mcp/McpClient.cpp
    src/mcp/McpToolAdapter.cpp
    src/security/PermissionManager.cpp
    src/skills/SkillCatalog.cpp
    src/storage/HistoryReader.cpp
    src/storage/JsonLogger.cpp
    src/tools/FileTools.cpp
    src/tools/ITool.cpp
    src/tools/PathUtils.cpp
    src/tools/ShellTool.cpp
    src/tools/ToolRegistry.cpp
    src/ui/Console.cpp
    src/main.cpp
)

target_include_directories(ai-agent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ai-agent PRIVATE
    cpr::cpr
    ftxui::component
    ftxui::dom
    ftxui::screen
    nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(ai-agent PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ai-agent PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(ai-agent-tests
    src/config/AppConfig.cpp
    src/agent/AgentLoop.cpp
    src/core/ContextManager.cpp
    src/core/Message.cpp
    src/core/Session.cpp
    src/diagnostics/Diagnostics.cpp
    src/llm/LlmParsing.cpp
    src/mcp/McpClient.cpp
    src/mcp/McpToolAdapter.cpp
    src/security/PermissionManager.cpp
    src/skills/SkillCatalog.cpp
    src/storage/HistoryReader.cpp
    src/storage/JsonLogger.cpp
    src/tools/FileTools.cpp
    src/tools/ITool.cpp
    src/tools/PathUtils.cpp
    src/tools/ShellTool.cpp
    src/tools/ToolRegistry.cpp
    tests/config_tests.cpp
    tests/diagnostics_tests.cpp
    tests/agent_loop_tests.cpp
    tests/context_manager_tests.cpp
    tests/history_tests.cpp
    tests/llm_parsing_tests.cpp
    tests/mcp_tests.cpp
    tests/security_tests.cpp
    tests/skill_catalog_tests.cpp
    tests/tools_tests.cpp
)

target_include_directories(ai-agent-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ai-agent-tests PRIVATE
    doctest::doctest
    nlohmann_json::nlohmann_json
)

add_test(NAME ai-agent-tests COMMAND ai-agent-tests)

if(MSVC)
    target_compile_options(ai-agent-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(ai-agent-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
