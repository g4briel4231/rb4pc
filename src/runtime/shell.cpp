#include "rb4pc/runtime/shell.hpp"

namespace rb4pc::runtime {
RuntimeShell::RuntimeShell() : menu_({"Songs", "Settings", "Diagnostics", "Quit"}) {}
void RuntimeShell::source_ready(bool ready) { screen_ = ready ? ShellScreen::main_menu : ShellScreen::diagnostics; }
void RuntimeShell::dispatch(ShellAction action) {
    if (screen_ == ShellScreen::main_menu) {
        if (action == ShellAction::up) menu_.move(-1);
        else if (action == ShellAction::down) menu_.move(1);
        else if (action == ShellAction::select) {
            switch (menu_.selected_index()) {
            case 0: screen_ = ShellScreen::song_browser; break;
            case 1: screen_ = ShellScreen::settings; break;
            case 2: screen_ = ShellScreen::diagnostics; break;
            default: screen_ = ShellScreen::quit; break;
            }
        }
    } else if (action == ShellAction::back && screen_ != ShellScreen::boot) screen_ = ShellScreen::main_menu;
}
ShellScreen RuntimeShell::screen() const { return screen_; }
const MenuModel& RuntimeShell::main_menu() const { return menu_; }
} // namespace rb4pc::runtime
