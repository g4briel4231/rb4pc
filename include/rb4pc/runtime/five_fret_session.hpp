#pragma once

#include "rb4pc/runtime/five_fret_gameplay.hpp"
#include "rb4pc/runtime/five_fret_input.hpp"
#include "rb4pc/runtime/note_highway.hpp"

#include <optional>

namespace rb4pc::runtime {

struct FiveFretFrameResult {
    FiveFretInputFrame input;
    std::optional<ChordHitResult> hit;
    double gameplay_seconds = 0;
    bool paused = false;
};

class FiveFretSession {
public:
    explicit FiveFretSession(TimedFiveFretChart chart, FiveFretInputProfile profile = default_xinput_five_fret_profile());
    [[nodiscard]] FiveFretFrameResult step(double delta_seconds, const ControllerState& controller);
    [[nodiscard]] HighwayRenderStats render(SoftwareFramebuffer& framebuffer, double past_window = 0.15, double future_window = 2.0) const;
    [[nodiscard]] const FiveFretGameplay& gameplay() const;
    [[nodiscard]] double seconds() const;
    [[nodiscard]] bool paused() const;
private:
    GameplayClock clock_;
    FiveFretGameplay gameplay_;
    FiveFretInputMapper input_;
    bool paused_ = false;
};

} // namespace rb4pc::runtime
