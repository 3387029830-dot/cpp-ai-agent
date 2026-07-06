#include <iostream>
#include <string>

int main() {
    std::cout << "cpp-ai-agent M0 is running.\n";
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

