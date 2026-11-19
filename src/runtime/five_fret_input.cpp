#include "rb4pc/runtime/five_fret_input.hpp"

#include <stdexcept>

namespace rb4pc::runtime {

FiveFretInputProfile default_xinput_five_fret_profile() {
    return {{0x1000, 0x2000, 0x8000, 0x4000, 0x0200}, 0x0003, 0x0010};
}

FiveFretInputMapper::FiveFretInputMapper(FiveFretInputProfile profile) : profile_(profile) {
    for (const auto button : profile_.fret_buttons)
        if (button == 0 || (button & (button - 1)) != 0) throw std::runtime_error("each fret must map to one button");
    if (profile_.strum_buttons == 0) throw std::runtime_error("strum mapping is required");
}

FiveFretInputFrame FiveFretInputMapper::sample(const ControllerState& state) {
    if (!state.connected) { reset(); return {}; }
    FiveFretInputFrame result; result.connected = true;
    for (std::uint8_t lane = 0; lane < profile_.fret_buttons.size(); ++lane)
        if ((state.buttons & profile_.fret_buttons[lane]) != 0)
            result.lane_mask = static_cast<std::uint8_t>(result.lane_mask | (1U << lane));
    const auto pressed_edges = static_cast<std::uint16_t>(state.buttons & ~previous_buttons_);
    result.strum_pressed = (pressed_edges & profile_.strum_buttons) != 0;
    result.pause_pressed = profile_.pause_button != 0 && (pressed_edges & profile_.pause_button) != 0;
    previous_buttons_ = state.buttons;
    return result;
}

void FiveFretInputMapper::reset() { previous_buttons_ = 0; }

} // namespace rb4pc::runtime
