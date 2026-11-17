#pragma once

#include "rb4pc/runtime/gameplay_timing.hpp"
#include "rb4pc/runtime/timed_chart.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace rb4pc::runtime {

enum class ChordState : std::uint8_t { pending, hit, missed };

struct ChordHitResult {
    std::size_t chord_index = 0;
    JudgementResult timing{Judgement::miss, 0, 0};
};

class FiveFretGameplay {
public:
    explicit FiveFretGameplay(TimedFiveFretChart chart);
    void update(double current_seconds);
    [[nodiscard]] std::optional<ChordHitResult> strum(std::uint8_t lane_mask, double input_seconds);
    [[nodiscard]] const TimedFiveFretChart& chart() const;
    [[nodiscard]] const std::vector<ChordState>& states() const;
    [[nodiscard]] std::size_t hit_count() const;
    [[nodiscard]] std::size_t miss_count() const;
    [[nodiscard]] std::uint64_t score() const;
    [[nodiscard]] bool finished() const;
private:
    TimedFiveFretChart chart_;
    std::vector<ChordState> states_;
    std::size_t next_chord_ = 0;
    std::size_t hit_count_ = 0;
    std::size_t miss_count_ = 0;
    std::uint64_t score_ = 0;
};

} // namespace rb4pc::runtime
