cmake_minimum_required(VERSION 3.20)

project(cpp_ai_agent
    VERSION 0.1.0
    DESCRIPTION "A terminal AI coding agent written in C++17"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ai-agent
    src/main.cpp
)

if(MSVC)
    target_compile_options(ai-agent PRIVATE /W4 /permissive-)
else()
    target_compile_options(ai-agent PRIVATE -Wall -Wextra -Wpedantic)
endif()

