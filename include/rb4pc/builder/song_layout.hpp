#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace rb4pc::builder {
struct CustomSongPackagePaths {
    std::filesystem::path header;
    std::filesystem::path archive;
};
[[nodiscard]] bool is_internal_song_path(std::string_view internal_path);
[[nodiscard]] CustomSongPackagePaths custom_song_package_paths(std::string_view short_name);
} // namespace rb4pc::builder
