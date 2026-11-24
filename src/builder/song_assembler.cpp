#include "rb4pc/builder/song_assembler.hpp"

#include "rb4pc/builder/song_layout.hpp"
#include "rb4pc/inspector/sha256.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace rb4pc::builder {
namespace {
SongAssemblyResult assemble_song_directory_impl(
    const std::filesystem::path& midi_path, const std::filesystem::path& mogg_path,
    const std::filesystem::path& mix_metadata_path,
    const std::optional<std::filesystem::path>& artwork_path,
    const std::filesystem::path& output_root, std::string short_name) {
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
    const auto destination_artwork = destination / (short_name + ".png_pc");
    const auto manifest = destination / "session.json";
    std::optional<inspector::TextureConversionResult> artwork_conversion;
    std::filesystem::create_directories(destination);
    try {
        std::filesystem::copy_file(midi_path, destination_midi);
        std::filesystem::copy_file(mogg_path, destination_mogg);
        std::filesystem::copy_file(mix_metadata_path, destination_mix);
        if (artwork_path)
            artwork_conversion = inspector::convert_linear_ps4_texture_to_pc(*artwork_path, destination_artwork);
        std::ofstream output(manifest, std::ios::binary);
        output << "{\n  \"schemaVersion\": " << (artwork_conversion ? 2 : 1)
               << ",\n  \"shortName\": \"" << short_name
               << "\",\n  \"files\": {\"midi\": \"notes.mid\", \"mogg\": \""
               << short_name << ".mogg\", \"mixMetadata\": \"" << short_name
               << ".mogg.dta\"},\n  \"hashes\": {\"midi\": \"" << session.midi.source_sha256
               << "\", \"mogg\": \"" << session.mogg_sha256
               << "\", \"mixMetadata\": \"" << session.mix_metadata_sha256 << "\"}";
        if (artwork_conversion) {
            output << ",\n  \"artwork\": {\"file\": \"" << destination_artwork.filename().string()
                   << "\", \"sha256\": \"" << artwork_conversion->destination_sha256
                   << "\", \"conversionManifest\": \"" << artwork_conversion->manifest.filename().string()
                   << "\", \"conversionManifestSha256\": \""
                   << inspector::sha256_file(artwork_conversion->manifest) << "\"}";
        }
        output << ",\n  \"readyForSongSession\": true\n}\n";
        if (!output) throw std::runtime_error("could not write assembled song manifest");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(destination, ignored);
        throw;
    }
    auto copied_bytes = std::filesystem::file_size(destination_midi) +
                        std::filesystem::file_size(destination_mogg) +
                        std::filesystem::file_size(destination_mix);
    if (artwork_conversion)
        copied_bytes += std::filesystem::file_size(destination_artwork) +
                        std::filesystem::file_size(artwork_conversion->manifest);
    SongAssemblyResult result;
    result.directory = destination;
    result.manifest = manifest;
    result.session = session;
    result.copied_bytes = copied_bytes;
    result.artwork_conversion = std::move(artwork_conversion);
    return result;
}
} // namespace

SongAssemblyResult assemble_song_directory(const std::filesystem::path& midi_path,
                                            const std::filesystem::path& mogg_path,
                                            const std::filesystem::path& mix_metadata_path,
                                            const std::filesystem::path& output_root,
                                            std::string short_name) {
    return assemble_song_directory_impl(
        midi_path, mogg_path, mix_metadata_path, std::nullopt, output_root, std::move(short_name));
}

SongAssemblyResult assemble_song_directory_with_artwork(
    const std::filesystem::path& midi_path, const std::filesystem::path& mogg_path,
    const std::filesystem::path& mix_metadata_path, const std::filesystem::path& artwork_path,
    const std::filesystem::path& output_root, std::string short_name) {
    return assemble_song_directory_impl(
        midi_path, mogg_path, mix_metadata_path, artwork_path, output_root, std::move(short_name));
}

} // namespace rb4pc::builder
