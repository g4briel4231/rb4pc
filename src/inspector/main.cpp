#include "rb4pc/inspector/file_inspector.hpp"

#include <exception>
#include <iostream>

namespace {

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " <file> [file ...]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 2;
    }

    bool failed = false;
    for (int index = 1; index < argc; ++index) {
        try {
            if (index > 1) {
                std::cout << '\n';
            }
            const auto report = rb4pc::inspector::inspect_file(argv[index]);
            std::cout << rb4pc::inspector::format_report(report);
        } catch (const std::exception& error) {
            std::cerr << "forge-inspector: " << error.what() << '\n';
            failed = true;
        }
    }

    return failed ? 1 : 0;
}

