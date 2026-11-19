#pragma once

#include <filesystem>

namespace rb4pc::runtime {

struct SongDirectoryFiles {
    std::filesystem::path directory;
    std::filesystem::path midi;
    std::filesystem::path mogg;
    std::filesystem::path mix_metadata;
};

[[nodiscard]] SongDirectoryFiles resolve_song_directory(const std::filesystem::path& directory);

} // namespace rb4pc::runtime
