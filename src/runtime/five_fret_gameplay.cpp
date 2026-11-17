#include "rb4pc/runtime/five_fret_gameplay.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace rb4pc::runtime {
namespace {
constexpr double hit_window_seconds = 0.100;
}

FiveFretGameplay::FiveFretGameplay(TimedFiveFretChart chart)
    : chart_(std::move(chart)), states_(chart_.chords.size(), ChordState::pending) {}

void FiveFretGameplay::update(double current_seconds) {
    if (current_seconds < 0) throw std::runtime_error("negative gameplay update time");
    while (next_chord_ < chart_.chords.size() &&
           current_seconds > chart_.chords[next_chord_].start_seconds + hit_window_seconds) {
        states_[next_chord_] = ChordState::missed;
        ++miss_count_; ++next_chord_;
    }
}

std::optional<ChordHitResult> FiveFretGameplay::strum(std::uint8_t lane_mask, double input_seconds) {
    if (lane_mask == 0 || (lane_mask & 0xe0U) != 0) throw std::runtime_error("invalid five-fret lane mask");
    update(input_seconds);
    if (next_chord_ >= chart_.chords.size()) return std::nullopt;
    const auto& chord = chart_.chords[next_chord_];
    if (std::abs(input_seconds - chord.start_seconds) > hit_window_seconds || lane_mask != chord.lane_mask)
        return std::nullopt;
    const auto result = judge_note(chord.start_seconds, input_seconds);
    const auto index = next_chord_++;
    states_[index] = ChordState::hit; ++hit_count_; score_ += result.score;
    return ChordHitResult{index, result};
}

const TimedFiveFretChart& FiveFretGameplay::chart() const { return chart_; }
const std::vector<ChordState>& FiveFretGameplay::states() const { return states_; }
std::size_t FiveFretGameplay::hit_count() const { return hit_count_; }
std::size_t FiveFretGameplay::miss_count() const { return miss_count_; }
std::uint64_t FiveFretGameplay::score() const { return score_; }
bool FiveFretGameplay::finished() const { return next_chord_ == chart_.chords.size(); }

} // namespace rb4pc::runtime
