#include "rb4pc/runtime/timed_chart.hpp"

#include <stdexcept>

namespace rb4pc::runtime {

TimedFiveFretChart time_five_fret_chart(const MidiFileSummary& midi, const FiveFretChart& chart) {
    TimedFiveFretChart result; result.difficulty = chart.difficulty; result.chords.reserve(chart.chords.size());
    for (const auto& chord : chart.chords) {
        if (chord.end_tick < chord.start_tick) throw std::runtime_error("five-fret chord ends before it starts");
        result.chords.push_back({chord.start_tick, chord.end_tick,
                                 midi_tick_to_seconds(midi, chord.start_tick),
                                 midi_tick_to_seconds(midi, chord.end_tick), chord.lane_mask});
    }
    return result;
}

std::vector<std::size_t> visible_five_fret_chords(
    const TimedFiveFretChart& chart,
    double current_seconds,
    double past_window_seconds,
    double future_window_seconds) {
    if (current_seconds < 0 || past_window_seconds < 0 || future_window_seconds < 0)
        throw std::runtime_error("negative chart window time");
    const auto first_visible = current_seconds - past_window_seconds;
    const auto last_visible = current_seconds + future_window_seconds;
    std::vector<std::size_t> result;
    for (std::size_t index = 0; index < chart.chords.size(); ++index) {
        const auto& chord = chart.chords[index];
        if (chord.start_seconds > last_visible) break;
        if (chord.end_seconds >= first_visible) result.push_back(index);
    }
    return result;
}

} // namespace rb4pc::runtime
