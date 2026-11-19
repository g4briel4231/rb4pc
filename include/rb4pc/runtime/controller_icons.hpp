#pragma once

#include "rb4pc/runtime/ui.hpp"

namespace rb4pc::runtime {

enum class ControllerIconTheme { xbox };
enum class ControllerPrompt {
    a,
    b,
    x,
    y,
    dpad,
    left_bumper,
    right_bumper,
    left_trigger,
    right_trigger,
    left_stick,
    right_stick,
    menu,
    view
};
enum class ControllerAction { confirm, cancel, navigate, previous_tab, next_tab, pause, secondary_view };

[[nodiscard]] constexpr ControllerIconTheme default_controller_icon_theme() {
    return ControllerIconTheme::xbox;
}

[[nodiscard]] ControllerPrompt controller_prompt_for_action(
    ControllerAction action,
    ControllerIconTheme theme = default_controller_icon_theme());

void render_controller_icon(SoftwareFramebuffer& framebuffer, ControllerPrompt prompt,
                            Rect bounds,
                            ControllerIconTheme theme = default_controller_icon_theme());

} // namespace rb4pc::runtime
