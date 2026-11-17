#pragma once

#include "rb4pc/runtime/midi_parser.hpp"

#include <cstdint>
#include <vector>

namespace rb4pc::runtime {

enum class ChartDifficulty : std::uint8_t { easy, medium, hard, expert };

struct FiveFretChord {
    std::uint64_t start_tick = 0;
    std::uint64_t end_tick = 0;
    std::uint8_t lane_mask = 0;
};

struct FiveFretChart {
    ChartDifficulty difficulty = ChartDifficulty::expert;
    std::vector<FiveFretChord> chords;
};

[[nodiscard]] FiveFretChart build_five_fret_chart(
    const MidiTrackSummary& track,
    ChartDifficulty difficulty);

} // namespace rb4pc::runtime
