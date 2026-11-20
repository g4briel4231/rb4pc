#include "rb4pc/inspector/texture_container.hpp"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc == 4 && std::string(argv[1]) == "--convert-pc") {
        try {
            const auto result = rb4pc::inspector::convert_linear_ps4_texture_to_pc(argv[2], argv[3]);
            std::cout << "Converted texture: " << result.destination.string() << '\n'
                      << "Storage: " << result.bits_per_pixel << " bpp\n"
                      << "Source SHA-256: " << result.source_sha256 << '\n'
                      << "Destination SHA-256: " << result.destination_sha256 << '\n'
                      << "Payload SHA-256: " << result.payload_sha256 << '\n'
                      << "Manifest: " << result.manifest.string() << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "texture-probe: " << error.what() << '\n';
            return 1;
        }
    }
    if (argc == 4 && std::string(argv[1]) == "--scan") {
        try {
            const auto corpus = rb4pc::inspector::inspect_texture_corpus(argv[2], argv[3]);
            std::cout << rb4pc::inspector::format_texture_corpus_report(corpus);
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "texture-probe: " << error.what() << '\n';
            return 1;
        }
    }
    if (argc != 2 && argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <texture-container> [reference-container]\n"
                  << "       " << argv[0] << " --scan <directory> <extension>\n";
        std::cerr << "       " << argv[0] << " --convert-pc <ps4-v12-container> <destination>\n";
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
