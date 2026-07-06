#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

int main() {
    std::cout << "cpp-ai-agent M0 is running.\n";
    const nlohmann::json runtimeInfo = {
        {"stage", "M0.5"},
        {"dependency_manager", "vcpkg"},
        {"json_library", "nlohmann-json"}
    };
    std::cout << "runtime> " << runtimeInfo.dump() << "\n";
    std::cout << "Type something and press Enter. Type /exit to quit.\n\n";

    std::string input;
    while (true) {
        std::cout << "input> ";

        if (!std::getline(std::cin, input)) {
            std::cout << "\n";
            break;
        }

        if (input == "/exit") {
            break;
        }

        if (input.empty()) {
            continue;
        }

        std::cout << "agent> M0 only echoes that the program is alive. You typed: "
                  << input << "\n";
    }

    std::cout << "bye\n";
    return 0;
}
