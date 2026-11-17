#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace rb4pc::inspector {
struct HeaderSetReport;
struct MusicTypeStats {
    std::size_t records = 0;
    std::uint64_t total_bytes = 0;
    std::uint32_t minimum_size = 0;
    std::uint32_t maximum_size = 0;
    std::size_t standard_midi_headers = 0;
    std::size_t ogg_page_headers = 0;
    std::map<std::string, std::size_t> first_four_bytes;
};
struct MusicCatalog { std::map<std::string, MusicTypeStats> types; std::size_t unreadable = 0; };
[[nodiscard]] MusicCatalog catalog_music_payloads(const HeaderSetReport& report);
[[nodiscard]] std::string format_report(const MusicCatalog& catalog);
} // namespace rb4pc::inspector
