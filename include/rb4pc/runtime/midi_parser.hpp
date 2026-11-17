#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rb4pc::runtime {
struct MidiHeader { std::uint16_t format = 0; std::uint16_t track_count = 0; std::uint16_t division = 0; std::uintmax_t file_size = 0; };
struct MidiNote {
    std::uint8_t channel = 0;
    std::uint8_t key = 0;
    std::uint8_t velocity = 0;
    std::uint64_t start_tick = 0;
    std::uint64_t end_tick = 0;
};
struct MidiTempoChange {
    std::uint64_t tick = 0;
    std::uint32_t microseconds_per_quarter = 500000;
};
struct MidiTrackSummary {
    std::string name;
    std::size_t event_count = 0;
    std::size_t note_on_count = 0;
    std::size_t note_off_count = 0;
    std::size_t unmatched_note_off_count = 0;
    std::size_t dangling_note_on_count = 0;
    std::uint64_t end_tick = 0;
    std::vector<MidiNote> notes;
};
struct MidiFileSummary {
    MidiHeader header;
    std::vector<MidiTrackSummary> tracks;
    std::size_t tempo_events = 0;
    std::vector<MidiTempoChange> tempo_changes;
};
[[nodiscard]] MidiHeader parse_standard_midi_header(const std::filesystem::path& path);
[[nodiscard]] MidiFileSummary parse_standard_midi(const std::filesystem::path& path);
[[nodiscard]] double midi_tick_to_seconds(const MidiFileSummary& midi, std::uint64_t tick);
} // namespace rb4pc::runtime
