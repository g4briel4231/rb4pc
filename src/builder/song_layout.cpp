#include "rb4pc/builder/song_layout.hpp"

#include <stdexcept>
#include <cctype>

namespace rb4pc::builder {
bool is_internal_song_path(std::string_view path) {
    return path.starts_with("songs/") || path.find("/songs/") != std::string_view::npos ||
           path.starts_with("songs_download/") || path.find("/songs_download/") != std::string_view::npos;
}

CustomSongPackagePaths custom_song_package_paths(std::string_view short_name) {
    if (short_name.empty()) throw std::runtime_error("custom-song short name is empty");
    for (const unsigned char character : short_name) {
        if (!std::isalnum(character) && character != '_' && character != '-') {
            throw std::runtime_error("custom-song short name contains an unsafe character");
        }
    }
    const std::string base(short_name);
    return {base + "_pc.hdr", base + "_pc_0.ark"};
}
} // namespace rb4pc::builder
