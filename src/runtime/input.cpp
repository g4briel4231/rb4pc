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
