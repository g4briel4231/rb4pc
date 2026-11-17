#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace rb4pc::builder {
struct CustomSongEntry {
    std::string short_name;
    std::filesystem::path header;
    std::filesystem::path archive;
    std::size_t record_count = 0;
    bool header_valid = false;
    bool archive_present = false;
    bool archive_usable = false;
    bool ready = false;
    std::string error;
};
struct CustomSongCatalog {
    std::filesystem::path installation;
    std::vector<CustomSongEntry> songs;
    std::size_t ready_songs = 0;
};
[[nodiscard]] CustomSongCatalog scan_custom_songs(const std::filesystem::path& installation);
[[nodiscard]] std::string format_json(const CustomSongCatalog& catalog);
} // namespace rb4pc::builder
