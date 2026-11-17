#pragma once
#include <cstdint>
#include <filesystem>

namespace rb4pc::runtime {
struct MidiHeader { std::uint16_t format = 0; std::uint16_t track_count = 0; std::uint16_t division = 0; std::uintmax_t file_size = 0; };
[[nodiscard]] MidiHeader parse_standard_midi_header(const std::filesystem::path& path);
} // namespace rb4pc::runtime
