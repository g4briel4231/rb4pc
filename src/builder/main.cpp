#include "rb4pc/builder/build_plan.hpp"
#include "rb4pc/builder/custom_song_catalog.hpp"
#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/payload_catalog.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 4 || (std::string(argv[2]) != "--plan" && std::string(argv[2]) != "--scan-custom-songs")) {
        std::cerr << "Usage: " << argv[0] << " <header.hdr> --plan <plan.json>\n"
                  << "   or: " << argv[0] << " <installation-dir> --scan-custom-songs <catalog.json>\n";
        return 2;
    }
    try {
        const std::filesystem::path output_path = argv[3];
        if (std::filesystem::exists(output_path)) throw std::runtime_error("plan destination already exists");
        if (std::string(argv[2]) == "--scan-custom-songs") {
            const auto catalog = rb4pc::builder::scan_custom_songs(argv[1]);
            std::ofstream output(output_path, std::ios::binary);
            if (!output) throw std::runtime_error("could not create custom-song catalog");
            output << rb4pc::builder::format_json(catalog);
            if (!output) throw std::runtime_error("could not write custom-song catalog");
            std::cout << "Custom-song catalog: " << output_path.string() << '\n'
                      << "Ready songs: " << catalog.ready_songs << '/' << catalog.songs.size() << '\n';
            return catalog.ready_songs == catalog.songs.size() ? 0 : 1;
        }
        const auto report = rb4pc::inspector::inspect_header_set(argv[1]);
        const auto catalog = rb4pc::inspector::catalog_payloads(report);
        const auto plan = rb4pc::builder::create_build_plan(report, catalog);
        std::ofstream output(output_path, std::ios::binary);
        if (!output) throw std::runtime_error("could not create build plan");
        output << rb4pc::builder::format_json(plan);
        if (!output) throw std::runtime_error("could not write build plan");
        std::cout << "Build plan: " << output_path.string() << '\n'
                  << "Ready for conversion planning: " << (plan.ready ? "yes" : "no") << '\n';
        return plan.ready ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "rb4pc-builder: " << error.what() << '\n';
        return 1;
    }
}
