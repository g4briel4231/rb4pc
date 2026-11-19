#include "rb4pc/builder/midi_session_plan.hpp"

#include "rb4pc/inspector/sha256.hpp"
#include "rb4pc/runtime/five_fret_chart.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace rb4pc::builder {
namespace {
MidiInstrumentPlan instrument_plan(const runtime::MidiFileSummary& midi, const std::string& name) {
    const auto found = std::find_if(midi.tracks.begin(), midi.tracks.end(), [&name](const auto& track) {
        return track.name == name;
    });
    if (found == midi.tracks.end()) return {name};
    const auto count = [&found](runtime::ChartDifficulty difficulty) {
        return runtime::build_five_fret_chart(*found, difficulty).chords.size();
    };
    return {name, count(runtime::ChartDifficulty::easy), count(runtime::ChartDifficulty::medium),
            count(runtime::ChartDifficulty::hard), count(runtime::ChartDifficulty::expert)};
}
}

MidiSessionPlan create_midi_session_plan(const runtime::MidiFileSummary& midi, std::string source_sha256) {
    MidiSessionPlan plan;
    plan.source_sha256 = std::move(source_sha256); plan.source_size = midi.header.file_size;
    plan.division = midi.header.division; plan.tempo_events = midi.tempo_events;
    std::uint64_t final_tick = 0;
    for (const auto& track : midi.tracks) final_tick = std::max(final_tick, track.end_tick);
    plan.duration_seconds = runtime::midi_tick_to_seconds(midi, final_tick);
    plan.guitar = instrument_plan(midi, "PART GUITAR");
    plan.bass = instrument_plan(midi, "PART BASS");
    plan.ready = plan.division != 0 && plan.tempo_events != 0 && plan.guitar.expert_chords != 0;
    return plan;
}

MidiSessionPlan create_midi_session_plan(const std::filesystem::path& midi_path) {
    return create_midi_session_plan(runtime::parse_standard_midi(midi_path), inspector::sha256_file(midi_path));
}

std::string format_json(const MidiSessionPlan& plan) {
    std::ostringstream output;
    const auto write_instrument = [&output](const MidiInstrumentPlan& instrument) {
        output << "{\"track\": \"" << instrument.track_name << "\", \"chords\": {\"easy\": "
               << instrument.easy_chords << ", \"medium\": " << instrument.medium_chords
               << ", \"hard\": " << instrument.hard_chords << ", \"expert\": "
               << instrument.expert_chords << "}}";
    };
    output << "{\n  \"schemaVersion\": 1,\n  \"source\": {\"sha256\": \"" << plan.source_sha256
           << "\", \"size\": " << plan.source_size << "},\n  \"midi\": {\"division\": "
           << plan.division << ", \"tempoEvents\": " << plan.tempo_events << ", \"durationSeconds\": "
           << plan.duration_seconds << "},\n  \"instruments\": {\"guitar\": ";
    write_instrument(plan.guitar); output << ", \"bass\": "; write_instrument(plan.bass);
    output << "},\n  \"readyForFiveFretSession\": " << (plan.ready ? "true" : "false") << "\n}\n";
    return output.str();
}

} // namespace rb4pc::builder
