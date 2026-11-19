#pragma once

#include "rb4pc/runtime/song_directory.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace rb4pc::runtime {

struct SongLibraryEntry {
    std::string short_name;
    std::filesystem::path directory;
    bool valid = false;
    std::string error;
    SongDirectoryFiles files;
};

struct SongLibrary {
    std::filesystem::path root;
    std::vector<SongLibraryEntry> entries;
    std::size_t valid_entries = 0;
};

[[nodiscard]] SongLibrary scan_song_library(const std::filesystem::path& root);

} // namespace rb4pc::runtime
