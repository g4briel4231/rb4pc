#pragma once

#include "rb4pc/runtime/five_fret_chart.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rb4pc::runtime {

struct TimedFiveFretChord {
    std::uint64_t start_tick = 0;
    std::uint64_t end_tick = 0;
    double start_seconds = 0;
    double end_seconds = 0;
    std::uint8_t lane_mask = 0;
};

struct TimedFiveFretChart {
    ChartDifficulty difficulty = ChartDifficulty::expert;
    std::vector<TimedFiveFretChord> chords;
};

[[nodiscard]] TimedFiveFretChart time_five_fret_chart(
    const MidiFileSummary& midi,
    const FiveFretChart& chart);

[[nodiscard]] std::vector<std::size_t> visible_five_fret_chords(
    const TimedFiveFretChart& chart,
    double current_seconds,
    double past_window_seconds,
    double future_window_seconds);

} // namespace rb4pc::runtime
