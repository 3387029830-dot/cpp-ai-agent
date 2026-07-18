#include <doctest/doctest.h>

#include "modes/ModeCatalog.h"

#include <filesystem>
#include <fstream>

TEST_CASE("mode catalog: load workflow and expert presets") {
    const auto path = std::filesystem::temp_directory_path() / "cpp_ai_agent_modes_test.json";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << R"({
          "workflows": [
            {
              "name": "plan",
              "description": "Plan then verify.",
              "suggested_prompt": "plan first",
              "system_prompt": "Use a plan.",
              "allowed_tools": ["read_file", "run_command"]
            }
          ],
          "experts": [
            {
              "name": "cpp",
              "description": "C++ expert.",
              "system_prompt": "Diagnose C++.",
              "skill": "cpp_debug"
            }
          ]
        })";
    }

    const auto catalog = cpp_ai_agent::modes::loadModeCatalog(path);
    REQUIRE(catalog.workflows().size() == 1);
    REQUIRE(catalog.experts().size() == 1);

    const auto* workflow = catalog.findWorkflow("plan");
    REQUIRE(workflow != nullptr);
    CHECK(workflow->allowedTools.size() == 2);
    CHECK(cpp_ai_agent::modes::modeKindToString(workflow->kind) == "workflow");

    const auto* expert = catalog.findExpert("cpp");
    REQUIRE(expert != nullptr);
    CHECK(expert->skillName == "cpp_debug");
    CHECK(cpp_ai_agent::modes::modeKindToString(expert->kind) == "expert");

    const auto message = cpp_ai_agent::modes::makeModeSystemMessage(*workflow, "src");
    CHECK(message.find("Active workflow: plan") != std::string::npos);
    CHECK(message.find("Target: src") != std::string::npos);
    CHECK(message.find("read_file") != std::string::npos);

    std::filesystem::remove(path);
}
