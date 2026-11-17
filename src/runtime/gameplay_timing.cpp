#include "rb4pc/runtime/gameplay_timing.hpp"
#include <cmath>
#include <stdexcept>

namespace rb4pc::runtime {
void GameplayClock::reset(double seconds) { if (seconds < 0) throw std::runtime_error("negative gameplay time"); seconds_ = seconds; paused_ = false; }
void GameplayClock::advance(double seconds) { if (seconds < 0) throw std::runtime_error("negative clock advance"); if (!paused_) seconds_ += seconds; }
void GameplayClock::set_paused(bool paused) { paused_ = paused; }
double GameplayClock::seconds() const { return seconds_; }
JudgementResult judge_note(double note, double input) {
    const auto error = std::abs(input - note);
    if (error <= 0.030) return {Judgement::perfect, error, 100};
    if (error <= 0.060) return {Judgement::great, error, 75};
    if (error <= 0.100) return {Judgement::good, error, 50};
    return {Judgement::miss, error, 0};
}
} // namespace rb4pc::runtime
