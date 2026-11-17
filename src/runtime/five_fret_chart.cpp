#include "rb4pc/runtime/five_fret_chart.hpp"

#include <algorithm>

namespace rb4pc::runtime {
namespace {
std::uint8_t first_key(ChartDifficulty difficulty) {
    switch (difficulty) {
        case ChartDifficulty::easy: return 60;
        case ChartDifficulty::medium: return 72;
        case ChartDifficulty::hard: return 84;
        case ChartDifficulty::expert: return 96;
    }
    return 96;
}
}

FiveFretChart build_five_fret_chart(const MidiTrackSummary& track, ChartDifficulty difficulty) {
    FiveFretChart result; result.difficulty = difficulty;
    const auto base = first_key(difficulty);
    std::vector<MidiNote> notes;
    for (const auto& note : track.notes) {
        if (note.key >= base && note.key < base + 5) notes.push_back(note);
    }
    std::sort(notes.begin(), notes.end(), [](const auto& left, const auto& right) {
        if (left.start_tick != right.start_tick) return left.start_tick < right.start_tick;
        return left.key < right.key;
    });
    for (const auto& note : notes) {
        const auto lane = static_cast<std::uint8_t>(note.key - base);
        if (result.chords.empty() || result.chords.back().start_tick != note.start_tick) {
            result.chords.push_back({note.start_tick, note.end_tick, static_cast<std::uint8_t>(1U << lane)});
        } else {
            auto& chord = result.chords.back();
            chord.end_tick = std::max(chord.end_tick, note.end_tick);
            chord.lane_mask = static_cast<std::uint8_t>(chord.lane_mask | (1U << lane));
        }
    }
    return result;
}

} // namespace rb4pc::runtime
