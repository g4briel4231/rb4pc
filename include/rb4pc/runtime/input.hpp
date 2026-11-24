#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace rb4pc::runtime {
struct ControllerState {
    std::uint32_t index = 0;
    bool connected = false;
    std::uint16_t buttons = 0;
    std::uint8_t left_trigger = 0;
    std::uint8_t right_trigger = 0;
    std::int16_t left_x = 0, left_y = 0, right_x = 0, right_y = 0;
};
struct KeyboardState {
    bool available = false;
    std::uint16_t navigation_buttons = 0;
    std::uint16_t gameplay_buttons = 0;
};
[[nodiscard]] bool input_backend_available();
[[nodiscard]] std::vector<ControllerState> poll_controllers();
[[nodiscard]] KeyboardState poll_keyboard();
[[nodiscard]] std::optional<ControllerState> select_connected_controller(
    const std::vector<ControllerState>& states,
    std::optional<std::uint32_t> preferred_index = std::nullopt);
} // namespace rb4pc::runtime
