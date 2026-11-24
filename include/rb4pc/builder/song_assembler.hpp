#pragma once

#include "rb4pc/builder/song_session_plan.hpp"
#include "rb4pc/inspector/texture_container.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace rb4pc::builder {

struct SongAssemblyResult {
    std::filesystem::path directory;
    std::filesystem::path manifest;
    SongSessionPlan session;
    std::uintmax_t copied_bytes = 0;
    std::optional<inspector::TextureConversionResult> artwork_conversion;
};

[[nodiscard]] SongAssemblyResult assemble_song_directory(
    const std::filesystem::path& midi_path, const std::filesystem::path& mogg_path,
    const std::filesystem::path& mix_metadata_path,
    const std::filesystem::path& output_root, std::string short_name);
[[nodiscard]] SongAssemblyResult assemble_song_directory_with_artwork(
    const std::filesystem::path& midi_path, const std::filesystem::path& mogg_path,
    const std::filesystem::path& mix_metadata_path, const std::filesystem::path& artwork_path,
    const std::filesystem::path& output_root, std::string short_name);

} // namespace rb4pc::builder
