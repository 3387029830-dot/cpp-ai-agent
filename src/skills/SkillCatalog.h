#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace cpp_ai_agent::skills {

struct Skill {
    std::string name;
    std::string description;
    std::string suggestedPrompt;
    std::string systemPrompt;
    std::vector<std::string> allowedTools;
};

class SkillCatalog {
public:
    explicit SkillCatalog(std::vector<Skill> skills);

    const Skill* find(const std::string& name) const;
    const std::vector<Skill>& all() const;

private:
    std::vector<Skill> skills_;
};

SkillCatalog loadSkillCatalog(const std::filesystem::path& path);
std::string makeSkillSystemMessage(const Skill& skill, const std::string& target = "");

}  // namespace cpp_ai_agent::skills
