#include "rb4pc/runtime/five_fret_import.hpp"
#include "rb4pc/runtime/five_fret_session.hpp"
#include "rb4pc/runtime/controller_icons.hpp"
#include "rb4pc/runtime/input.hpp"
#include "rb4pc/runtime/mogg_audio_output.hpp"
#include "rb4pc/runtime/mogg_parser.hpp"
#include "rb4pc/runtime/song_directory.hpp"
#include "rb4pc/runtime/window.hpp"
#include "rb4pc/runtime/ui.hpp"

#include <atomic>
#include <array>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <song-directory>\n"
                  << "   or: " << argv[0] << " <notes.mid> <song.mogg> <song.mogg.dta>\n";
        return 2;
    }
    try {
        const auto files = argc == 2 ? rb4pc::runtime::resolve_song_directory(argv[1])
                                     : rb4pc::runtime::SongDirectoryFiles{{}, argv[1], argv[2], argv[3]};
        const auto imported = rb4pc::runtime::import_five_fret_midi(
            files.midi, "PART GUITAR", rb4pc::runtime::ChartDifficulty::expert);
        const auto mogg = rb4pc::runtime::parse_mogg_header(files.mogg);
        const auto vorbis = rb4pc::runtime::inspect_ogg_vorbis(files.mogg, mogg.ogg_offset);
        auto profile = rb4pc::runtime::default_xinput_five_fret_profile();
        profile.pause_button = 0;
        rb4pc::runtime::FiveFretSession session(imported.chart, profile);
        rb4pc::runtime::SoftwareFramebuffer framebuffer(1280, 720);
        std::atomic<std::uint64_t> completed_frames = 0;
        std::atomic<bool> cancel = false, playback_done = false, playback_failed = false;
        std::mutex error_mutex;
        std::string playback_error;
        std::optional<std::uint32_t> active_controller_index;
        std::thread audio([&] {
            try {
                rb4pc::runtime::play_mogg_blocking(
                    files.mogg, files.mix_metadata, {},
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
            rb4pc::runtime::run_window_loop("RB4PC v0.49.0 Interactive Song", framebuffer, [&] {
                const auto controllers = rb4pc::runtime::poll_controllers();
                const auto selected = rb4pc::runtime::select_connected_controller(
                    controllers, active_controller_index);
                if (selected) active_controller_index = selected->index;
                else active_controller_index.reset();
                const auto controller = selected.value_or(rb4pc::runtime::ControllerState{});
                const auto seconds = static_cast<double>(completed_frames.load()) / vorbis.sample_rate;
                static_cast<void>(session.step_to(seconds, controller));
                static_cast<void>(session.render(framebuffer));
                rb4pc::runtime::fill_rect(
                    framebuffer, {0, 0, framebuffer.width(), 8},
                    selected ? 0xff2a9d50 : 0xffb43b3b);
                constexpr std::array prompts{
                    rb4pc::runtime::ControllerPrompt::a,
                    rb4pc::runtime::ControllerPrompt::b,
                    rb4pc::runtime::ControllerPrompt::y,
                    rb4pc::runtime::ControllerPrompt::x,
                    rb4pc::runtime::ControllerPrompt::right_bumper};
                for (std::size_t index = 0; index < prompts.size(); ++index)
                    rb4pc::runtime::render_controller_icon(
                        framebuffer, prompts[index],
                        {20 + static_cast<std::uint32_t>(index) * 44, 20, 32, 32});
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
