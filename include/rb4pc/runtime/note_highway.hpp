#pragma once

#include "rb4pc/runtime/timed_chart.hpp"
#include "rb4pc/runtime/window.hpp"

#include <cstddef>

namespace rb4pc::runtime {

struct HighwayRenderOptions {
    double current_seconds = 0;
    double past_window_seconds = 0.15;
    double future_window_seconds = 2.0;
};

struct HighwayRenderStats {
    std::size_t visible_chords = 0;
    std::size_t rendered_gems = 0;
    std::size_t rendered_sustains = 0;
};

[[nodiscard]] HighwayRenderStats render_note_highway(
    SoftwareFramebuffer& framebuffer,
    const TimedFiveFretChart& chart,
    const HighwayRenderOptions& options);

} // namespace rb4pc::runtime
