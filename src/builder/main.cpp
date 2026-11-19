#include "rb4pc/builder/build_plan.hpp"
#include "rb4pc/builder/custom_song_catalog.hpp"
#include "rb4pc/builder/install_plan.hpp"
#include "rb4pc/builder/midi_session_plan.hpp"
#include "rb4pc/builder/song_session_plan.hpp"
#include "rb4pc/builder/song_assembler.hpp"
#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/payload_catalog.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc == 7 && std::string(argv[2]) == "--assemble-song") {
        try {
            const auto result = rb4pc::builder::assemble_song_directory(
                argv[1], argv[3], argv[4], argv[5], argv[6]);
            std::cout << "Assembled song: " << result.directory.string()
                      << "\nManifest: " << result.manifest.string()
                      << "\nCopied bytes: " << result.copied_bytes << '\n';
            return 0;
        } catch (const std::exception& error) { std::cerr << "rb4pc-builder: " << error.what() << '\n'; return 1; }
    }
    if (argc == 6 && std::string(argv[2]) == "--plan-song-session") {
        try {
            const std::filesystem::path output_path = argv[5];
            if (std::filesystem::exists(output_path)) throw std::runtime_error("plan destination already exists");
            const auto plan = rb4pc::builder::create_song_session_plan(argv[1], argv[3], argv[4]);
            std::ofstream output(output_path, std::ios::binary); output << rb4pc::builder::format_json(plan);
            if (!output) throw std::runtime_error("could not write song session plan");
            std::cout << "Song session plan: " << output_path.string() << '\n'
                      << "Ready for song session: " << (plan.ready ? "yes" : "no") << '\n';
            return plan.ready ? 0 : 1;
        } catch (const std::exception& error) { std::cerr << "rb4pc-builder: " << error.what() << '\n'; return 1; }
    }
    if (argc == 4 && std::string(argv[2]) == "--plan-midi-session") {
        try {
            const std::filesystem::path output_path = argv[3];
            if (std::filesystem::exists(output_path)) throw std::runtime_error("plan destination already exists");
            const auto plan = rb4pc::builder::create_midi_session_plan(argv[1]);
            std::ofstream output(output_path, std::ios::binary); output << rb4pc::builder::format_json(plan);
            if (!output) throw std::runtime_error("could not write MIDI session plan");
            std::cout << "MIDI session plan: " << output_path.string() << '\n'
                      << "Ready for five-fret session: " << (plan.ready ? "yes" : "no") << '\n';
            return plan.ready ? 0 : 1;
        } catch (const std::exception& error) { std::cerr << "rb4pc-builder: " << error.what() << '\n'; return 1; }
    }
    if (argc == 5 && std::string(argv[2]) == "--plan-song-install") {
        try {
            const std::filesystem::path output_path = argv[4];
            if (std::filesystem::exists(output_path)) throw std::runtime_error("plan destination already exists");
            const auto catalog = rb4pc::builder::scan_custom_songs(argv[1]);
            const auto plan = rb4pc::builder::create_install_plan(catalog, argv[3]);
            std::ofstream output(output_path, std::ios::binary); output << rb4pc::builder::format_json(plan);
            if (!output) throw std::runtime_error("could not write install plan");
            std::cout << "Install plan: " << output_path.string() << '\n' << "Operations: " << plan.operations.size()
                      << "\nCollisions: " << plan.collisions << '\n'; return 0;
        } catch (const std::exception& error) { std::cerr << "rb4pc-builder: " << error.what() << '\n'; return 1; }
    }
    if (argc != 4 || (std::string(argv[2]) != "--plan" && std::string(argv[2]) != "--scan-custom-songs")) {
        std::cerr << "Usage: " << argv[0] << " <header.hdr> --plan <plan.json>\n"
                  << "   or: " << argv[0] << " <installation-dir> --scan-custom-songs <catalog.json>\n";
        std::cerr << "   or: " << argv[0] << " <source-dir> --plan-song-install <target-dir> <plan.json>\n";
        std::cerr << "   or: " << argv[0] << " <notes.mid> --plan-midi-session <session.json>\n";
        std::cerr << "   or: " << argv[0] << " <notes.mid> --plan-song-session <song.mogg> <song.mogg.dta> <session.json>\n";
        std::cerr << "   or: " << argv[0] << " <notes.mid> --assemble-song <song.mogg> <song.mogg.dta> <output-root> <shortname>\n";
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
