#include "rb4pc/runtime/five_fret_import.hpp"
#include "rb4pc/runtime/five_fret_session.hpp"
#include "rb4pc/runtime/input.hpp"
#include "rb4pc/runtime/mogg_audio_output.hpp"
#include "rb4pc/runtime/mogg_parser.hpp"
#include "rb4pc/runtime/window.hpp"

#include <atomic>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <notes.mid> <song.mogg> <song.mogg.dta>\n";
        return 2;
    }
    try {
        const auto imported = rb4pc::runtime::import_five_fret_midi(
            argv[1], "PART GUITAR", rb4pc::runtime::ChartDifficulty::expert);
        const auto mogg = rb4pc::runtime::parse_mogg_header(argv[2]);
        const auto vorbis = rb4pc::runtime::inspect_ogg_vorbis(argv[2], mogg.ogg_offset);
        auto profile = rb4pc::runtime::default_xinput_five_fret_profile();
        profile.pause_button = 0;
        rb4pc::runtime::FiveFretSession session(imported.chart, profile);
        rb4pc::runtime::SoftwareFramebuffer framebuffer(1280, 720);
        std::atomic<std::uint64_t> completed_frames = 0;
        std::atomic<bool> cancel = false, playback_done = false, playback_failed = false;
        std::mutex error_mutex;
        std::string playback_error;
        std::thread audio([&] {
            try {
                rb4pc::runtime::play_mogg_blocking(
                    argv[2], argv[3], {},
                    [&](std::uint64_t frames) { completed_frames.store(frames); },
                    [&] { return cancel.load(); });
            } catch (const std::exception& error) {
                std::lock_guard lock(error_mutex);
                playback_error = error.what();
                playback_failed.store(true);
            }
            playback_done.store(true);
        });
        try {
            rb4pc::runtime::run_window_loop("RB4PC v0.48.0 Interactive Song", framebuffer, [&] {
                const auto controllers = rb4pc::runtime::poll_controllers();
                const auto controller = controllers.empty() ? rb4pc::runtime::ControllerState{} : controllers[0];
                const auto seconds = static_cast<double>(completed_frames.load()) / vorbis.sample_rate;
                static_cast<void>(session.step_to(seconds, controller));
                static_cast<void>(session.render(framebuffer));
                return !playback_done.load() && !playback_failed.load();
            });
        } catch (...) {
            cancel.store(true);
            audio.join();
            throw;
        }
        cancel.store(true);
        audio.join();
        if (playback_failed.load()) {
            std::lock_guard lock(error_mutex);
            throw std::runtime_error(playback_error);
        }
        std::cout << "Song session ended at " << session.seconds() << " seconds\n"
                  << "Hits: " << session.gameplay().hit_count()
                  << "\nMisses: " << session.gameplay().miss_count()
                  << "\nScore: " << session.gameplay().score() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "rb4pc-song-play: " << error.what() << '\n';
        return 1;
    }
}
