#include "rb4pc/runtime/input.hpp"

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
} // namespace rb4pc::runtime
