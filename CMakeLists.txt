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

add_executable(ai-agent
    src/config/AppConfig.cpp
    src/core/Message.cpp
    src/core/Session.cpp
    src/llm/LlmClient.cpp
    src/tools/FileTools.cpp
    src/tools/PathUtils.cpp
    src/tools/ShellTool.cpp
    src/tools/ToolRegistry.cpp
    src/main.cpp
)

target_include_directories(ai-agent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ai-agent PRIVATE
    cpr::cpr
    nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(ai-agent PRIVATE /W4 /permissive-)
else()
    target_compile_options(ai-agent PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(ai-agent-tests
    src/tools/FileTools.cpp
    src/tools/PathUtils.cpp
    src/tools/ShellTool.cpp
    src/tools/ToolRegistry.cpp
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
