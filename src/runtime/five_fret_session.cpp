#include "rb4pc/runtime/five_fret_session.hpp"

#include <stdexcept>
#include <utility>

namespace rb4pc::runtime {

FiveFretSession::FiveFretSession(TimedFiveFretChart chart, FiveFretInputProfile profile)
    : gameplay_(std::move(chart)), input_(profile) {}

FiveFretFrameResult FiveFretSession::step(double delta_seconds, const ControllerState& controller) {
    if (delta_seconds < 0) throw std::runtime_error("negative session frame delta");
    FiveFretFrameResult result; result.input = input_.sample(controller);
    if (result.input.pause_pressed) {
        paused_ = !paused_; clock_.set_paused(paused_);
    }
    clock_.advance(delta_seconds);
    if (!paused_) {
        gameplay_.update(clock_.seconds());
        if (result.input.strum_pressed) result.hit = gameplay_.strum(result.input.lane_mask, clock_.seconds());
    }
    result.gameplay_seconds = clock_.seconds(); result.paused = paused_;
    return result;
}

FiveFretFrameResult FiveFretSession::step_to(double transport_seconds, const ControllerState& controller) {
    if (transport_seconds < clock_.seconds()) throw std::runtime_error("five-fret session transport moved backwards");
    return step(transport_seconds - clock_.seconds(), controller);
}

HighwayRenderStats FiveFretSession::render(SoftwareFramebuffer& framebuffer, double past_window, double future_window) const {
    return render_note_highway(framebuffer, gameplay_.chart(), {clock_.seconds(), past_window, future_window});
}

const FiveFretGameplay& FiveFretSession::gameplay() const { return gameplay_; }
double FiveFretSession::seconds() const { return clock_.seconds(); }
bool FiveFretSession::paused() const { return paused_; }

} // namespace rb4pc::runtime
