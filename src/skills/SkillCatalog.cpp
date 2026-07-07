#include "skills/SkillCatalog.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

namespace cpp_ai_agent::skills {

SkillCatalog::SkillCatalog(std::vector<Skill> skills) : skills_(std::move(skills)) {}

const Skill* SkillCatalog::find(const std::string& name) const {
    for (const auto& skill : skills_) {
        if (skill.name == name) {
            return &skill;
        }
    }
    return nullptr;
}

const std::vector<Skill>& SkillCatalog::all() const {
    return skills_;
}

SkillCatalog loadSkillCatalog(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open skills file: " + path.string());
    }

    nlohmann::json json;
    input >> json;

    std::vector<Skill> skills;
    for (const auto& rawSkill : json.value("skills", nlohmann::json::array())) {
        Skill skill;
        skill.name = rawSkill.value("name", "");
        skill.description = rawSkill.value("description", "");
        skill.suggestedPrompt = rawSkill.value("suggested_prompt", "");
        skill.systemPrompt = rawSkill.value("system_prompt", "");
        for (const auto& rawTool : rawSkill.value("allowed_tools", nlohmann::json::array())) {
            skill.allowedTools.push_back(rawTool.get<std::string>());
        }
        if (!skill.name.empty()) {
            skills.push_back(std::move(skill));
        }
    }
    return SkillCatalog(std::move(skills));
}

std::string makeSkillSystemMessage(const Skill& skill, const std::string& target) {
    std::string message = "Active skill: " + skill.name + ". ";
    if (!skill.description.empty()) {
        message += skill.description + " ";
    }
    if (!target.empty()) {
        message += "Target: " + target + ". ";
    }
    if (!skill.allowedTools.empty()) {
        message += "Allowed tools for this skill: ";
        for (std::size_t i = 0; i < skill.allowedTools.size(); ++i) {
            if (i > 0) {
                message += ", ";
            }
            message += skill.allowedTools.at(i);
        }
        message += ". Do not call tools outside this list. ";
    }
    if (!skill.systemPrompt.empty()) {
        message += "Follow these skill instructions: " + skill.systemPrompt;
    }
    return message;
}

}  // namespace cpp_ai_agent::skills
