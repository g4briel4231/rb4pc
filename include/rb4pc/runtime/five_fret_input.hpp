#pragma once

#include "rb4pc/runtime/input.hpp"

#include <array>
#include <cstdint>

namespace rb4pc::runtime {

struct FiveFretInputProfile {
    std::array<std::uint16_t, 5> fret_buttons{};
    std::uint16_t strum_buttons = 0;
    std::uint16_t pause_button = 0;
};

struct FiveFretInputFrame {
    bool connected = false;
    std::uint8_t lane_mask = 0;
    bool strum_pressed = false;
    bool pause_pressed = false;
};

[[nodiscard]] FiveFretInputProfile default_xinput_five_fret_profile();

class FiveFretInputMapper {
public:
    explicit FiveFretInputMapper(FiveFretInputProfile profile = default_xinput_five_fret_profile());
    [[nodiscard]] FiveFretInputFrame sample(const ControllerState& state);
    void reset();
private:
    FiveFretInputProfile profile_;
    std::uint16_t previous_buttons_ = 0;
};

} // namespace rb4pc::runtime
