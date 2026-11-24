#pragma once

#include <filesystem>
#include <optional>

namespace rb4pc::runtime {

struct SongDirectoryFiles {
    std::filesystem::path directory;
    std::filesystem::path midi;
    std::filesystem::path mogg;
    std::filesystem::path mix_metadata;
    std::filesystem::path manifest;
    std::optional<std::filesystem::path> artwork;
    std::optional<std::filesystem::path> artwork_conversion_manifest;
    bool integrity_verified = false;
    bool artwork_integrity_verified = false;
};

[[nodiscard]] SongDirectoryFiles resolve_song_directory(const std::filesystem::path& directory);

} // namespace rb4pc::runtime
