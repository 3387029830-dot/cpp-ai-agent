#include <doctest/doctest.h>

#include "skills/SkillCatalog.h"

#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path makeTempSkillFile(const std::string& name) {
    auto root = std::filesystem::temp_directory_path() / ("cpp-ai-agent-skills-" + name);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root / "skills.json";
}

void writeTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
}

}  // namespace

TEST_CASE("SkillCatalog loads and finds configured skills") {
    const auto path = makeTempSkillFile("load");
    writeTextFile(
        path,
        R"({
  "skills": [
    {
      "name": "code_review",
      "description": "Review code.",
      "allowed_tools": ["read_file"],
      "suggested_prompt": "Review src.",
      "system_prompt": "Find bugs first."
    }
  ]
})"
    );

    const auto catalog = cpp_ai_agent::skills::loadSkillCatalog(path);
    REQUIRE(catalog.all().size() == 1);

    const auto* skill = catalog.find("code_review");
    REQUIRE(skill != nullptr);
    CHECK(skill->description == "Review code.");
    REQUIRE(skill->allowedTools.size() == 1);
    CHECK(skill->allowedTools.at(0) == "read_file");
    CHECK(skill->suggestedPrompt == "Review src.");
    const auto systemMessage = cpp_ai_agent::skills::makeSkillSystemMessage(*skill, "src/mcp");
    CHECK(systemMessage.find("Find bugs first.") != std::string::npos);
    CHECK(systemMessage.find("Target: src/mcp") != std::string::npos);
    CHECK(systemMessage.find("read_file") != std::string::npos);

    std::filesystem::remove_all(path.parent_path());
}

TEST_CASE("SkillCatalog ignores unnamed skill entries") {
    const auto path = makeTempSkillFile("unnamed");
    writeTextFile(path, R"({"skills":[{"description":"missing name"}]})");

    const auto catalog = cpp_ai_agent::skills::loadSkillCatalog(path);

    CHECK(catalog.all().empty());
    CHECK(catalog.find("missing") == nullptr);

    std::filesystem::remove_all(path.parent_path());
}
