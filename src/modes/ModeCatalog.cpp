#include "modes/ModeCatalog.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

namespace cpp_ai_agent::modes {

namespace {

nlohmann::json loadJsonFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open modes file: " + path.string());
    }

    nlohmann::json json;
    input >> json;
    return json;
}

ModePreset parseModePreset(const nlohmann::json& raw, ModeKind kind) {
    ModePreset mode;
    mode.kind = kind;
    mode.name = raw.value("name", "");
    mode.description = raw.value("description", "");
    mode.suggestedPrompt = raw.value("suggested_prompt", "");
    mode.systemPrompt = raw.value("system_prompt", "");
    mode.skillName = raw.value("skill", raw.value("skill_name", ""));
    for (const auto& rawTool : raw.value("allowed_tools", nlohmann::json::array())) {
        mode.allowedTools.push_back(rawTool.get<std::string>());
    }
    return mode;
}

}  // namespace

ModeCatalog::ModeCatalog(std::vector<ModePreset> workflows, std::vector<ModePreset> experts)
    : workflows_(std::move(workflows)), experts_(std::move(experts)) {}

const ModePreset* ModeCatalog::findWorkflow(const std::string& name) const {
    for (const auto& mode : workflows_) {
        if (mode.name == name) {
            return &mode;
        }
    }
    return nullptr;
}

const ModePreset* ModeCatalog::findExpert(const std::string& name) const {
    for (const auto& mode : experts_) {
        if (mode.name == name) {
            return &mode;
        }
    }
    return nullptr;
}

const std::vector<ModePreset>& ModeCatalog::workflows() const {
    return workflows_;
}

const std::vector<ModePreset>& ModeCatalog::experts() const {
    return experts_;
}

std::vector<ModePreset> ModeCatalog::all() const {
    std::vector<ModePreset> combined;
    combined.reserve(workflows_.size() + experts_.size());
    combined.insert(combined.end(), workflows_.begin(), workflows_.end());
    combined.insert(combined.end(), experts_.begin(), experts_.end());
    return combined;
}

ModeCatalog loadModeCatalog(const std::filesystem::path& path) {
    const auto json = loadJsonFile(path);

    std::vector<ModePreset> workflows;
    for (const auto& rawMode : json.value("workflows", nlohmann::json::array())) {
        auto mode = parseModePreset(rawMode, ModeKind::Workflow);
        if (!mode.name.empty()) {
            workflows.push_back(std::move(mode));
        }
    }

    std::vector<ModePreset> experts;
    for (const auto& rawMode : json.value("experts", nlohmann::json::array())) {
        auto mode = parseModePreset(rawMode, ModeKind::Expert);
        if (!mode.name.empty()) {
            experts.push_back(std::move(mode));
        }
    }

    return ModeCatalog(std::move(workflows), std::move(experts));
}

std::string modeKindToString(ModeKind kind) {
    switch (kind) {
        case ModeKind::Workflow:
            return "workflow";
        case ModeKind::Expert:
            return "expert";
    }
    return "workflow";
}

std::string makeModeSystemMessage(const ModePreset& mode, const std::string& target) {
    std::string message = "Active " + modeKindToString(mode.kind) + ": " + mode.name + ". ";
    if (!mode.description.empty()) {
        message += mode.description + " ";
    }
    if (!target.empty()) {
        message += "Target: " + target + ". ";
    }
    if (!mode.allowedTools.empty()) {
        message += "Allowed tools for this mode: ";
        for (std::size_t i = 0; i < mode.allowedTools.size(); ++i) {
            if (i > 0) {
                message += ", ";
            }
            message += mode.allowedTools.at(i);
        }
        message += ". ";
    }
    if (!mode.suggestedPrompt.empty()) {
        message += "Suggested prompt: " + mode.suggestedPrompt + " ";
    }
    if (!mode.systemPrompt.empty()) {
        message += "Follow these mode instructions: " + mode.systemPrompt;
    }
    if (mode.kind == ModeKind::Expert && !mode.skillName.empty()) {
        message += " Bundled skill: " + mode.skillName + ".";
    }
    return message;
}

}  // namespace cpp_ai_agent::modes
