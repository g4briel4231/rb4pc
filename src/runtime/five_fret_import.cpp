#include "rb4pc/runtime/five_fret_import.hpp"

#include "rb4pc/runtime/five_fret_chart.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rb4pc::runtime {

ImportedFiveFretChart import_five_fret_midi(
    const std::filesystem::path& midi_path,
    std::string track_name,
    ChartDifficulty difficulty) {
    auto midi = parse_standard_midi(midi_path);
    const auto track = std::find_if(midi.tracks.begin(), midi.tracks.end(), [&track_name](const auto& candidate) {
        return candidate.name == track_name;
    });
    if (track == midi.tracks.end()) throw std::runtime_error("requested five-fret MIDI track was not found");
    auto chart = build_five_fret_chart(*track, difficulty);
    if (chart.chords.empty()) throw std::runtime_error("requested five-fret MIDI track has no chords at this difficulty");
    auto timed_chart = time_five_fret_chart(midi, chart);
    return {std::move(track_name), std::move(midi), std::move(timed_chart)};
}

} // namespace rb4pc::runtime
