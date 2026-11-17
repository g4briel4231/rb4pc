#pragma once
#include <cstdint>

namespace rb4pc::runtime {
class GameplayClock {
public:
    void reset(double seconds = 0);
    void advance(double seconds);
    void set_paused(bool paused);
    [[nodiscard]] double seconds() const;
private:
    double seconds_ = 0;
    bool paused_ = false;
};
enum class Judgement { perfect, great, good, miss };
struct JudgementResult { Judgement judgement; double absolute_error; std::uint32_t score; };
[[nodiscard]] JudgementResult judge_note(double note_seconds, double input_seconds);
} // namespace rb4pc::runtime
