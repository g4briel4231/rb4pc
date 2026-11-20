#include "rb4pc/runtime/interactive_song.hpp"

#include "rb4pc/runtime/controller_icons.hpp"
#include "rb4pc/runtime/five_fret_import.hpp"
#include "rb4pc/runtime/five_fret_session.hpp"
#include "rb4pc/runtime/input.hpp"
#include "rb4pc/runtime/mogg_audio_output.hpp"
#include "rb4pc/runtime/mogg_parser.hpp"
#include "rb4pc/runtime/ui.hpp"
#include "rb4pc/runtime/window.hpp"

#include <array>
#include <atomic>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace rb4pc::runtime {

InteractiveSongResult run_interactive_song(const SongDirectoryFiles& files) {
    const auto imported = import_five_fret_midi(
        files.midi, "PART GUITAR", ChartDifficulty::expert);
    const auto mogg = parse_mogg_header(files.mogg);
    const auto vorbis = inspect_ogg_vorbis(files.mogg, mogg.ogg_offset);
    FiveFretSession session(imported.chart);
    SoftwareFramebuffer framebuffer(1280, 720);
    std::atomic<std::uint64_t> completed_frames = 0;
    std::atomic<bool> cancel = false, pause_audio = false;
    std::atomic<bool> playback_done = false, playback_failed = false;
    std::atomic<float> guitar_level = 1.0F;
    std::mutex error_mutex;
    std::string playback_error;
    std::optional<std::uint32_t> active_controller_index;
    std::size_t observed_misses = 0;
    std::size_t guitar_mute_events = 0;
    std::thread audio([&] {
        try {
            play_mogg_blocking(
                files.mogg, files.mix_metadata, {},
                [&](std::uint64_t frames) { completed_frames.store(frames); },
                [&] { return cancel.load(); },
                [&] { return pause_audio.load(); },
                [&] {
                    MoggMixConfig config;
                    config.stem_levels["guitar"] = guitar_level.load();
                    return config;
                });
        } catch (const std::exception& error) {
            std::lock_guard lock(error_mutex);
            playback_error = error.what();
            playback_failed.store(true);
        }
        playback_done.store(true);
    });
    try {
        run_window_loop("RB4PC v0.57.0 Interactive Song", framebuffer, [&] {
            const auto controllers = poll_controllers();
            const auto selected = select_connected_controller(controllers, active_controller_index);
            if (selected) active_controller_index = selected->index;
            else active_controller_index.reset();
            const auto controller = selected.value_or(ControllerState{});
            const auto seconds = static_cast<double>(completed_frames.load()) / vorbis.sample_rate;
            const auto frame = session.step_to(seconds, controller);
            if (session.gameplay().miss_count() > observed_misses) {
                observed_misses = session.gameplay().miss_count();
                if (guitar_level.load() >= 1.0F) ++guitar_mute_events;
                guitar_level.store(0.25F);
            }
            if (frame.hit) guitar_level.store(1.0F);
            pause_audio.store(session.paused());
            static_cast<void>(session.render(framebuffer));
            fill_rect(framebuffer, {0, 0, framebuffer.width(), 8},
                      selected ? 0xff2a9d50 : 0xffb43b3b);
            fill_rect(framebuffer, {0, 8, framebuffer.width(), 5},
                      guitar_level.load() < 1.0F ? 0xffd08a28 : 0xff315b83);
            constexpr std::array prompts{ControllerPrompt::a, ControllerPrompt::b,
                                         ControllerPrompt::y, ControllerPrompt::x,
                                         ControllerPrompt::right_bumper};
            for (std::size_t index = 0; index < prompts.size(); ++index)
                render_controller_icon(framebuffer, prompts[index],
                                       {20 + static_cast<std::uint32_t>(index) * 44, 20, 32, 32});
            return !playback_done.load() && !playback_failed.load();
        });
    } catch (...) {
        cancel.store(true);
        audio.join();
        throw;
    }
    const auto cancelled = !playback_done.load();
    cancel.store(true);
    audio.join();
    if (playback_failed.load()) {
        std::lock_guard lock(error_mutex);
        throw std::runtime_error(playback_error);
    }
    return {session.seconds(), session.gameplay().hit_count(), session.gameplay().miss_count(),
            session.gameplay().score(), guitar_mute_events, cancelled};
}

} // namespace rb4pc::runtime
