#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace cpp_ai_agent::modes {

enum class ModeKind {
    Workflow,
    Expert,
};

struct ContractField {
    std::string key;
    std::string label;
    std::string placeholder;
};

struct ModePreset {
    ModeKind kind = ModeKind::Workflow;
    std::string name;
    std::string description;
    std::string suggestedPrompt;
    std::string systemPrompt;
    std::string contractTemplate;
    std::vector<ContractField> contractSchema;
    std::string skillName;
    std::vector<std::string> allowedTools;
};

class ModeCatalog {
public:
    explicit ModeCatalog(std::vector<ModePreset> workflows, std::vector<ModePreset> experts);

    const ModePreset* findWorkflow(const std::string& name) const;
    const ModePreset* findExpert(const std::string& name) const;
    const std::vector<ModePreset>& workflows() const;
    const std::vector<ModePreset>& experts() const;
    std::vector<ModePreset> all() const;

private:
    std::vector<ModePreset> workflows_;
    std::vector<ModePreset> experts_;
};

ModeCatalog loadModeCatalog(const std::filesystem::path& path);
std::string modeKindToString(ModeKind kind);
std::string makeModeSystemMessage(const ModePreset& mode, const std::string& target = "");

}  // namespace cpp_ai_agent::modes
