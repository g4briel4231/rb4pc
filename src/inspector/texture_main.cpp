#include "rb4pc/inspector/texture_container.hpp"

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <texture-container> [reference-container]\n";
        return 2;
    }
    try {
        const auto source = rb4pc::inspector::inspect_texture_container(argv[1]);
        std::cout << rb4pc::inspector::format_texture_report(source);
        if (argc == 3) {
            const auto reference = rb4pc::inspector::inspect_texture_container(argv[2]);
            std::cout << rb4pc::inspector::format_texture_report(reference);
            const auto comparison = rb4pc::inspector::compare_texture_payloads(source, reference);
            std::cout << "Payload compatible: " << (comparison.compatible ? "yes" : "no") << '\n'
                      << "Reason: " << comparison.reason << '\n';
            return comparison.compatible ? 0 : 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "texture-probe: " << error.what() << '\n';
        return 1;
    }
}
