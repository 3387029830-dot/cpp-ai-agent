#include "tools/ToolRegistry.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace cpp_ai_agent::tools {

void ToolRegistry::registerTool(std::shared_ptr<ITool> tool) {
    if (tool == nullptr) {
        throw std::invalid_argument("Cannot register a null tool.");
    }

    const auto toolName = tool->name();
    if (toolName.empty()) {
        throw std::invalid_argument("Cannot register a tool with an empty name.");
    }

    if (tools_.find(toolName) != tools_.end()) {
        throw std::invalid_argument("Tool already registered: " + toolName);
    }

    tools_.emplace(toolName, std::move(tool));
}

const ITool* ToolRegistry::find(const std::string& name) const {
    const auto it = tools_.find(name);
    if (it == tools_.end()) {
        return nullptr;
    }

    return it->second.get();
}

std::vector<std::string> ToolRegistry::names() const {
    std::vector<std::string> result;
    result.reserve(tools_.size());

    for (const auto& [name, tool] : tools_) {
        result.push_back(name);
    }

    std::sort(result.begin(), result.end());
    return result;
}

nlohmann::json ToolRegistry::toolsSpec() const {
    nlohmann::json result = nlohmann::json::array();

    for (const auto& name : names()) {
        const auto* tool = find(name);
        result.push_back({
            {"type", "function"},
            {"function",
             {
                 {"name", tool->name()},
                 {"description", tool->description()},
                 {"parameters", tool->parametersSchema()},
             }},
        });
    }

    return result;
}

}  // namespace cpp_ai_agent::tools
