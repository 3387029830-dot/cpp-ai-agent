#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "tools/FileTools.h"
#include "tools/PathUtils.h"
#include "tools/ToolRegistry.h"
#include "tools/WebSearchTool.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace {

class DummyTool final : public cpp_ai_agent::tools::ITool {
public:
    std::string name() const override {
        return "dummy";
    }

    std::string description() const override {
        return "A dummy test tool.";
    }

    nlohmann::json parametersSchema() const override {
        return {
            {"type", "object"},
            {"properties", nlohmann::json::object()},
        };
    }

    cpp_ai_agent::tools::RiskLevel risk() const override {
        return cpp_ai_agent::tools::RiskLevel::Safe;
    }

    cpp_ai_agent::tools::ToolResult execute(const nlohmann::json&) const override {
        return {true, "ok", "ok"};
    }
};

std::filesystem::path makeTempWorkspace(const std::string& name) {
    auto root = std::filesystem::temp_directory_path() / ("cpp-ai-agent-" + name);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void writeTextFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
}

}  // namespace

TEST_CASE("ToolRegistry registers and finds tools") {
    cpp_ai_agent::tools::ToolRegistry registry;

    registry.registerTool(std::make_shared<DummyTool>());

    CHECK(registry.find("dummy") != nullptr);
    CHECK(registry.find("missing") == nullptr);
    CHECK(registry.names() == std::vector<std::string>{"dummy"});

    const auto spec = registry.toolsSpec();
    REQUIRE(spec.is_array());
    REQUIRE(spec.size() == 1);
    CHECK(spec.at(0).at("function").at("name") == "dummy");
}

TEST_CASE("ToolRegistry rejects duplicate names") {
    cpp_ai_agent::tools::ToolRegistry registry;

    registry.registerTool(std::make_shared<DummyTool>());

    CHECK_THROWS_AS(registry.registerTool(std::make_shared<DummyTool>()), std::invalid_argument);
}

TEST_CASE("PathUtils blocks paths outside workspace") {
    const auto root = makeTempWorkspace("path-utils");
    writeTextFile(root / "inside.txt", "hello");

    const auto inside = cpp_ai_agent::tools::resolveWorkspacePath(root, "inside.txt");
    CHECK(cpp_ai_agent::tools::isPathInsideWorkspace(root, inside));

    CHECK_THROWS(cpp_ai_agent::tools::resolveWorkspacePath(root, "../outside.txt"));

    std::filesystem::remove_all(root);
}

TEST_CASE("File tools read, write, and edit files inside workspace") {
    const auto root = makeTempWorkspace("file-tools");

    cpp_ai_agent::tools::WriteFileTool writeTool(root);
    cpp_ai_agent::tools::ReadFileTool readTool(root);
    cpp_ai_agent::tools::ListDirTool listTool(root);
    cpp_ai_agent::tools::EditFileTool editTool(root);

    const auto writeResult = writeTool.execute({
        {"path", "notes/example.txt"},
        {"content", "hello world"},
    });
    REQUIRE(writeResult.success);

    const auto readResult = readTool.execute({{"path", "notes/example.txt"}});
    REQUIRE(readResult.success);
    CHECK(readResult.output == "hello world");

    const auto listResult = listTool.execute({{"path", "notes"}});
    REQUIRE(listResult.success);
    CHECK(listResult.output.find("[file] notes/example.txt") != std::string::npos);

    const auto readDirResult = readTool.execute({{"path", "notes"}});
    CHECK_FALSE(readDirResult.success);
    CHECK(readDirResult.output.find("Use list_dir") != std::string::npos);

    const auto editResult = editTool.execute({
        {"path", "notes/example.txt"},
        {"old_text", "world"},
        {"new_text", "agent"},
    });
    REQUIRE(editResult.success);

    const auto preview = editTool.preview({
        {"path", "notes/example.txt"},
        {"old_text", "agent"},
        {"new_text", "assistant"},
    });
    CHECK(preview.find("diff preview") != std::string::npos);
    CHECK(preview.find("-agent") != std::string::npos);
    CHECK(preview.find("+assistant") != std::string::npos);

    const auto editedResult = readTool.execute({{"path", "notes/example.txt"}});
    REQUIRE(editedResult.success);
    CHECK(editedResult.output == "hello agent");

    std::filesystem::remove_all(root);
}

TEST_CASE("File tools reject paths outside workspace") {
    const auto root = makeTempWorkspace("file-tools-outside");
    cpp_ai_agent::tools::ReadFileTool readTool(root);
    cpp_ai_agent::tools::ListDirTool listTool(root);

    const auto result = readTool.execute({{"path", "../outside.txt"}});

    CHECK_FALSE(result.success);
    CHECK(result.output.find("outside the workspace") != std::string::npos);

    const auto listResult = listTool.execute({{"path", "../"}});
    CHECK_FALSE(listResult.success);
    CHECK(listResult.output.find("outside the workspace") != std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("WebSearchTool formats structured search results") {
    const nlohmann::json body = {
        {"Heading", "C++"},
        {"AbstractText", "C++ is a general-purpose programming language."},
        {"AbstractURL", "https://example.test/cpp"},
        {"RelatedTopics",
         nlohmann::json::array({
             {
                 {"Text", "CMake - build system generator"},
                 {"FirstURL", "https://example.test/cmake"},
             },
             {
                 {"Name", "Nested"},
                 {"Topics",
                  nlohmann::json::array({
                      {
                          {"Text", "vcpkg - package manager"},
                          {"FirstURL", "https://example.test/vcpkg"},
                      },
                  })},
             },
         })},
    };

    const auto formatted = cpp_ai_agent::tools::formatDuckDuckGoResults("cpp build", body, 2);

    CHECK(formatted.find("Query: cpp build") != std::string::npos);
    CHECK(formatted.find("Top answer") != std::string::npos);
    CHECK(formatted.find("1. CMake - build system generator") != std::string::npos);
    CHECK(formatted.find("2. vcpkg - package manager") != std::string::npos);
    CHECK(formatted.find("整理建议") != std::string::npos);

    const auto fallback = cpp_ai_agent::tools::formatDuckDuckGoResults("cpp agent + mcp", nlohmann::json::object(), 2);
    CHECK(fallback.find("Search URL: https://duckduckgo.com/?q=cpp%20agent%20%2B%20mcp") != std::string::npos);
}
