#include "rb4pc/runtime/song_library.hpp"

#include <algorithm>
#include <stdexcept>

namespace rb4pc::runtime {

SongLibrary scan_song_library(const std::filesystem::path& root) {
    if (!std::filesystem::is_directory(root)) throw std::runtime_error("song library root is not a directory");
    SongLibrary result;
    result.root = std::filesystem::weakly_canonical(root);
    std::vector<std::filesystem::path> directories;
    for (const auto& entry : std::filesystem::directory_iterator(result.root))
        if (entry.is_directory()) directories.push_back(entry.path());
    std::sort(directories.begin(), directories.end(), [](const auto& left, const auto& right) {
        return left.filename().string() < right.filename().string();
    });
    for (const auto& directory : directories) {
        SongLibraryEntry entry;
        entry.short_name = directory.filename().string();
        entry.directory = directory;
        try {
            entry.files = resolve_song_directory(directory);
            entry.valid = entry.files.integrity_verified;
            if (entry.valid) ++result.valid_entries;
        } catch (const std::exception& error) {
            entry.error = error.what();
        }
        result.entries.push_back(std::move(entry));
    }
    return result;
}

} // namespace rb4pc::runtime
