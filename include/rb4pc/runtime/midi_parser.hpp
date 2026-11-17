#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rb4pc::runtime {
struct MidiHeader { std::uint16_t format = 0; std::uint16_t track_count = 0; std::uint16_t division = 0; std::uintmax_t file_size = 0; };
struct MidiTrackSummary {
    std::string name;
    std::size_t event_count = 0;
    std::size_t note_on_count = 0;
    std::size_t note_off_count = 0;
    std::uint64_t end_tick = 0;
};
struct MidiFileSummary { MidiHeader header; std::vector<MidiTrackSummary> tracks; std::size_t tempo_events = 0; };
[[nodiscard]] MidiHeader parse_standard_midi_header(const std::filesystem::path& path);
[[nodiscard]] MidiFileSummary parse_standard_midi(const std::filesystem::path& path);
} // namespace rb4pc::runtime
