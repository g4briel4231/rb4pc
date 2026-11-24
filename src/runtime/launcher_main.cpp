#include "rb4pc/runtime/app_config.hpp"
#include "rb4pc/runtime/input.hpp"
#include "rb4pc/runtime/interactive_song.hpp"
#include "rb4pc/runtime/song_browser.hpp"
#include "rb4pc/runtime/song_library.hpp"
#include "rb4pc/runtime/window.hpp"

#include <iostream>
#include <optional>
#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [songs-directory]\n";
        return 2;
    }
    try {
        const std::filesystem::path songs_directory = argc == 2 ? argv[1] : "songs";
        const auto config = rb4pc::runtime::load_or_create_app_config("rb4pc.ini");
        const auto library = rb4pc::runtime::scan_song_library(songs_directory);
        rb4pc::runtime::SongBrowserModel browser(library);
        rb4pc::runtime::SoftwareFramebuffer framebuffer(1280, 720);
        std::optional<std::uint32_t> active_controller;
        std::optional<std::filesystem::path> selected_directory;
        std::uint16_t previous_buttons = 0;
        rb4pc::runtime::run_window_loop("RB4PC v0.160.0 Song Library", framebuffer, [&] {
            const auto selected_controller = rb4pc::runtime::select_connected_controller(
                rb4pc::runtime::poll_controllers(), active_controller);
            if (selected_controller) active_controller = selected_controller->index;
            else active_controller.reset();
            const auto keyboard = rb4pc::runtime::poll_keyboard();
            const auto buttons = static_cast<std::uint16_t>(
                (selected_controller ? selected_controller->buttons : 0) | keyboard.navigation_buttons);
            const auto pressed = static_cast<std::uint16_t>(buttons & ~previous_buttons);
            previous_buttons = buttons;
            if ((pressed & 0x0001U) != 0) browser.move(-1);
            if ((pressed & 0x0002U) != 0) browser.move(1);
            if ((pressed & 0x1000U) != 0 && !browser.empty()) {
                selected_directory = browser.selected().directory;
                return false;
            }
            if ((pressed & 0x2000U) != 0) return false;
            rb4pc::runtime::render_song_browser(
                framebuffer, library, browser, selected_controller.has_value() || keyboard.available);
            return true;
        });
        if (!selected_directory) return 0;
        const auto files = rb4pc::runtime::resolve_song_directory(*selected_directory);
        const auto result = rb4pc::runtime::run_interactive_song(files, config);
        std::cout << "Song: " << selected_directory->filename().string()
                  << "\nHits: " << result.hits << "\nMisses: " << result.misses
                  << "\nScore: " << result.score << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "rb4pc-launcher: " << error.what() << '\n';
        return 1;
    }
}
