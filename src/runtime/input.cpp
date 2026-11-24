#include "rb4pc/runtime/input.hpp"

#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Xinput.h>
#endif

namespace rb4pc::runtime {
bool input_backend_available() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}
std::vector<ControllerState> poll_controllers() {
    std::vector<ControllerState> result;
#ifdef _WIN32
    result.reserve(XUSER_MAX_COUNT);
    for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index) {
        XINPUT_STATE state{};
        const auto connected = XInputGetState(index, &state) == ERROR_SUCCESS;
        ControllerState item; item.index = index; item.connected = connected;
        if (connected) {
            item.buttons = state.Gamepad.wButtons;
            item.left_trigger = state.Gamepad.bLeftTrigger; item.right_trigger = state.Gamepad.bRightTrigger;
            item.left_x = state.Gamepad.sThumbLX; item.left_y = state.Gamepad.sThumbLY;
            item.right_x = state.Gamepad.sThumbRX; item.right_y = state.Gamepad.sThumbRY;
        }
        result.push_back(item);
    }
#endif
    return result;
}

KeyboardState poll_keyboard() {
    KeyboardState result;
#ifdef _WIN32
    result.available = true;
    const auto pressed = [](int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; };
    if (pressed(VK_UP)) {
        result.navigation_buttons |= 0x0001;
        result.gameplay_buttons |= 0x0001;
    }
    if (pressed(VK_DOWN)) {
        result.navigation_buttons |= 0x0002;
        result.gameplay_buttons |= 0x0002;
    }
    if (pressed(VK_RETURN)) result.navigation_buttons |= 0x1000;
    if (pressed(VK_ESCAPE)) result.navigation_buttons |= 0x2000;
    if (pressed('1')) result.gameplay_buttons |= 0x1000;
    if (pressed('2')) result.gameplay_buttons |= 0x2000;
    if (pressed('4')) result.gameplay_buttons |= 0x4000;
    if (pressed('3')) result.gameplay_buttons |= 0x8000;
    if (pressed('5')) result.gameplay_buttons |= 0x0200;
    if (pressed('P')) result.gameplay_buttons |= 0x0010;
#endif
    return result;
}

std::optional<ControllerState> select_connected_controller(
    const std::vector<ControllerState>& states,
    std::optional<std::uint32_t> preferred_index) {
    if (preferred_index) {
        const auto preferred = std::find_if(states.begin(), states.end(), [&](const auto& state) {
            return state.connected && state.index == *preferred_index;
        });
        if (preferred != states.end()) return *preferred;
    }
    const auto first = std::find_if(states.begin(), states.end(), [](const auto& state) {
        return state.connected;
    });
    return first == states.end() ? std::nullopt : std::optional<ControllerState>(*first);
}
} // namespace rb4pc::runtime
