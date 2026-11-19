#include "rb4pc/builder/song_assembler.hpp"

#include "rb4pc/builder/song_layout.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace rb4pc::builder {

SongAssemblyResult assemble_song_directory(const std::filesystem::path& midi_path,
                                            const std::filesystem::path& mogg_path,
                                            const std::filesystem::path& mix_metadata_path,
                                            const std::filesystem::path& output_root,
                                            std::string short_name) {
    static_cast<void>(custom_song_package_paths(short_name));
    if (output_root.empty()) throw std::runtime_error("song assembly output root is empty");
    const auto session = create_song_session_plan(midi_path, mogg_path, mix_metadata_path);
    if (!session.ready) throw std::runtime_error("source files are not ready for a complete song session");
    const auto destination = output_root / "songs" / short_name;
    if (std::filesystem::exists(destination))
        throw std::runtime_error("song assembly destination already exists");

    const auto destination_midi = destination / "notes.mid";
    const auto destination_mogg = destination / (short_name + ".mogg");
    const auto destination_mix = destination / (short_name + ".mogg.dta");
    const auto manifest = destination / "session.json";
    std::filesystem::create_directories(destination);
    try {
        std::filesystem::copy_file(midi_path, destination_midi);
        std::filesystem::copy_file(mogg_path, destination_mogg);
        std::filesystem::copy_file(mix_metadata_path, destination_mix);
        std::ofstream output(manifest, std::ios::binary);
        output << "{\n  \"schemaVersion\": 1,\n  \"shortName\": \"" << short_name
               << "\",\n  \"files\": {\"midi\": \"notes.mid\", \"mogg\": \""
               << short_name << ".mogg\", \"mixMetadata\": \"" << short_name
               << ".mogg.dta\"},\n  \"hashes\": {\"midi\": \"" << session.midi.source_sha256
               << "\", \"mogg\": \"" << session.mogg_sha256
               << "\", \"mixMetadata\": \"" << session.mix_metadata_sha256
               << "\"},\n  \"readyForSongSession\": true\n}\n";
        if (!output) throw std::runtime_error("could not write assembled song manifest");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(destination, ignored);
        throw;
    }
    const auto copied_bytes = std::filesystem::file_size(destination_midi) +
                              std::filesystem::file_size(destination_mogg) +
                              std::filesystem::file_size(destination_mix);
    return {destination, manifest, session, copied_bytes};
}

} // namespace rb4pc::builder
