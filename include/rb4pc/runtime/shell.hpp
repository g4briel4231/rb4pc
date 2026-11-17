#pragma once
#include "rb4pc/runtime/ui.hpp"

namespace rb4pc::runtime {
enum class ShellScreen { boot, main_menu, song_browser, settings, diagnostics, quit };
enum class ShellAction { up, down, select, back };
class RuntimeShell {
public:
    RuntimeShell();
    void source_ready(bool ready);
    void dispatch(ShellAction action);
    [[nodiscard]] ShellScreen screen() const;
    [[nodiscard]] const MenuModel& main_menu() const;
private:
    ShellScreen screen_ = ShellScreen::boot;
    MenuModel menu_;
};
} // namespace rb4pc::runtime
