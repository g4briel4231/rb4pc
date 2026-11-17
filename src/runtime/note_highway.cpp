#include "rb4pc/runtime/note_highway.hpp"

#include "rb4pc/runtime/ui.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace rb4pc::runtime {
namespace {
constexpr std::array<std::uint32_t, 5> lane_colors = {
    0xff36b45a, 0xffd93a3a, 0xffe2c23a, 0xff3b78dc, 0xffe88735};
}

HighwayRenderStats render_note_highway(
    SoftwareFramebuffer& fb,
    const TimedFiveFretChart& chart,
    const HighwayRenderOptions& options) {
    if (options.current_seconds < 0 || options.past_window_seconds <= 0 || options.future_window_seconds <= 0)
        throw std::runtime_error("invalid note-highway time window");
    fb.clear(0xff0d1016);
    const auto highway_width = std::min<std::uint32_t>(fb.width(), 300);
    const auto lane_width = highway_width / 5;
    if (lane_width == 0 || fb.height() < 16) return {};
    const auto highway_x = (fb.width() - lane_width * 5) / 2;
    const auto hit_y = fb.height() - 24;
    fill_rect(fb, {highway_x, 0, lane_width * 5, fb.height()}, 0xff181d27);
    for (std::uint32_t lane = 0; lane <= 5; ++lane)
        fill_rect(fb, {highway_x + std::min(lane, 5U) * lane_width, 0, 1, fb.height()}, 0xff424b5c);
    fill_rect(fb, {highway_x, hit_y, lane_width * 5, 3}, 0xfff0f2f5);

    const auto y_for_time = [&](double seconds) {
        const auto delta = seconds - options.current_seconds;
        const auto pixels = delta >= 0
            ? delta / options.future_window_seconds * static_cast<double>(hit_y)
            : delta / options.past_window_seconds * 21.0;
        return static_cast<int>(std::lround(static_cast<double>(hit_y) - pixels));
    };
    const auto visible = visible_five_fret_chords(chart, options.current_seconds,
                                                   options.past_window_seconds,
                                                   options.future_window_seconds);
    HighwayRenderStats stats; stats.visible_chords = visible.size();
    for (const auto index : visible) {
        const auto& chord = chart.chords[index];
        const auto gem_y = std::clamp(y_for_time(chord.start_seconds), 0, static_cast<int>(fb.height()) - 1);
        const auto end_y = std::clamp(y_for_time(chord.end_seconds), 0, static_cast<int>(fb.height()) - 1);
        for (std::uint32_t lane = 0; lane < 5; ++lane) {
            if ((chord.lane_mask & (1U << lane)) == 0) continue;
            const auto x = highway_x + lane * lane_width;
            if (chord.end_seconds > chord.start_seconds) {
                const auto top = static_cast<std::uint32_t>(std::min(gem_y, end_y));
                const auto height = static_cast<std::uint32_t>(std::abs(end_y - gem_y) + 1);
                fill_rect(fb, {x + lane_width / 2 - 3, top, 6, height}, lane_colors[lane] & 0xff9f9f9fU);
                ++stats.rendered_sustains;
            }
            const auto y = static_cast<std::uint32_t>(gem_y > 4 ? gem_y - 4 : 0);
            fill_rect(fb, {x + 3, y, lane_width > 6 ? lane_width - 6 : lane_width, 9}, lane_colors[lane]);
            ++stats.rendered_gems;
        }
    }
    return stats;
}

} // namespace rb4pc::runtime
