#pragma once

#include "rb4pc/runtime/timed_chart.hpp"

#include <filesystem>
#include <string>

namespace rb4pc::runtime {

struct ImportedFiveFretChart {
    std::string track_name;
    MidiFileSummary midi;
    TimedFiveFretChart chart;
};

[[nodiscard]] ImportedFiveFretChart import_five_fret_midi(
    const std::filesystem::path& midi_path,
    std::string track_name,
    ChartDifficulty difficulty);

} // namespace rb4pc::runtime
