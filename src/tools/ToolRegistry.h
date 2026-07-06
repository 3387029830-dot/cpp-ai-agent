#pragma once

#include "tools/ITool.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cpp_ai_agent::tools {

class ToolRegistry {
public:
    void registerTool(std::shared_ptr<ITool> tool);

    const ITool* find(const std::string& name) const;
    std::vector<std::string> names() const;
    nlohmann::json toolsSpec() const;

private:
    std::unordered_map<std::string, std::shared_ptr<ITool>> tools_;
};

}  // namespace cpp_ai_agent::tools
