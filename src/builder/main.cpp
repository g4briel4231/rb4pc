#include "rb4pc/builder/build_plan.hpp"
#include "rb4pc/builder/custom_song_catalog.hpp"
#include "rb4pc/builder/install_plan.hpp"
#include "rb4pc/builder/midi_session_plan.hpp"
#include "rb4pc/builder/song_session_plan.hpp"
#include "rb4pc/builder/song_assembler.hpp"
#include "rb4pc/builder/pc_archive_builder.hpp"
#include "rb4pc/builder/pc_asset_catalog.hpp"
#include "rb4pc/builder/pc_audio_catalog.hpp"
#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/payload_catalog.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc == 4 && std::string(argv[2]) == "--catalog-pc-audio") {
        try {
            const std::filesystem::path output_path = argv[3];
            if (std::filesystem::exists(output_path)) throw std::runtime_error("catalog destination already exists");
            const auto catalog = rb4pc::builder::catalog_pc_audio(argv[1]);
            std::ofstream output(output_path, std::ios::binary);
            if (!output) throw std::runtime_error("could not create PC audio catalog");
            output << rb4pc::builder::format_json(catalog);
            if (!output) throw std::runtime_error("could not finalize PC audio catalog");
            std::cout << "PC audio catalog: " << output_path.string()
                      << "\nValid HDRs: " << catalog.valid_headers << '/' << catalog.candidate_headers
                      << "\nAudio records: " << catalog.audio_records
                      << "\nMOGG v10/Ogg: " << catalog.mogg_version_10 << '/' << catalog.mogg_embedded_ogg
                      << "\nRIFF WAVE/FEV: " << catalog.riff_wave << '/' << catalog.riff_fev << '\n';
            return catalog.invalid_headers == 0 && catalog.unreadable_records == 0 ? 0 : 1;
        } catch (const std::exception& error) { std::cerr << "rb4pc-builder: " << error.what() << '\n'; return 1; }
    }
    if (argc == 4 && std::string(argv[2]) == "--catalog-pc-assets") {
        try {
            const std::filesystem::path output_path = argv[3];
            if (std::filesystem::exists(output_path)) throw std::runtime_error("catalog destination already exists");
            const auto catalog = rb4pc::builder::catalog_pc_assets(argv[1]);
            std::ofstream output(output_path, std::ios::binary);
            if (!output) throw std::runtime_error("could not create PC asset catalog");
            output << rb4pc::builder::format_json(catalog);
            if (!output) throw std::runtime_error("could not finalize PC asset catalog");
            std::cout << "PC asset catalog: " << output_path.string()
                      << "\nValid HDRs: " << catalog.valid_headers << '/' << catalog.candidate_headers
                      << "\nRecords: " << catalog.records
                      << "\nNative/intermediate/unknown path families: " << catalog.native_path_records << '/'
                      << catalog.intermediate_path_records << '/' << catalog.unknown_path_records << '\n';
            return catalog.invalid_headers == 0 ? 0 : 1;
        } catch (const std::exception& error) { std::cerr << "rb4pc-builder: " << error.what() << '\n'; return 1; }
    }
    if ((argc == 5 || argc == 6) && std::string(argv[2]) == "--build-pc-archive") {
        try {
            rb4pc::builder::PcArchiveBuildOptions options;
            if (argc == 6) {
                const std::string value = argv[5];
                std::size_t processed = 0;
                const auto parsed = std::stoull(value, &processed);
                if (processed != value.size())
                    throw std::runtime_error("maximum ARK bytes must be a decimal integer");
                if (parsed == 0 || parsed > 0xffffffffULL)
                    throw std::runtime_error("maximum ARK bytes must be between 1 and 4294967295");
                options.maximum_archive_bytes = static_cast<std::uint32_t>(parsed);
            }
            const auto result = rb4pc::builder::build_pc_song_archive(argv[1], argv[3], argv[4], options);
            std::cout << "PC HDR: " << result.header.string()
                      << "\nPC ARK volumes: " << result.archives.size()
                      << "\nRecords: " << result.record_count
                      << "\nPayload bytes: " << result.payload_bytes
                      << "\nHDR SHA-256: " << result.header_sha256 << '\n';
            for (std::size_t index = 0; index < result.archives.size(); ++index)
                std::cout << "ARK[" << index << "]: " << result.archives[index].string()
                          << "\nARK[" << index << "] SHA-256: " << result.archive_sha256[index] << '\n';
            return 0;
        } catch (const std::exception& error) { std::cerr << "rb4pc-builder: " << error.what() << '\n'; return 1; }
    }
    if (argc == 8 && std::string(argv[2]) == "--assemble-song-with-art") {
        try {
            const auto result = rb4pc::builder::assemble_song_directory_with_artwork(
                argv[1], argv[3], argv[4], argv[5], argv[6], argv[7]);
            std::cout << "Assembled song: " << result.directory.string()
                      << "\nManifest: " << result.manifest.string()
                      << "\nArtwork: " << result.artwork_conversion->destination.string()
                      << "\nCopied/generated bytes: " << result.copied_bytes << '\n';
            return 0;
        } catch (const std::exception& error) { std::cerr << "rb4pc-builder: " << error.what() << '\n'; return 1; }
    }
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
        std::cerr << "   or: " << argv[0] << " <notes.mid> --assemble-song-with-art <song.mogg> <song.mogg.dta> <art.png_ps4> <output-root> <shortname>\n";
        std::cerr << "   or: " << argv[0] << " <song-directory> --build-pc-archive <output-dir> <shortname> [max-ark-bytes]\n";
        std::cerr << "   or: " << argv[0] << " <hdr-or-directory> --catalog-pc-assets <catalog.json>\n";
        std::cerr << "   or: " << argv[0] << " <hdr-or-directory> --catalog-pc-audio <catalog.json>\n";
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
