#pragma once

#include "rb4pc/runtime/midi_parser.hpp"

#include <cstddef>
#include <string>

namespace rb4pc::builder {

struct MidiInstrumentPlan {
    std::string track_name;
    std::size_t easy_chords = 0;
    std::size_t medium_chords = 0;
    std::size_t hard_chords = 0;
    std::size_t expert_chords = 0;
    double expert_chart_end_seconds = 0.0;
};

struct MidiSessionPlan {
    std::string source_sha256;
    std::uintmax_t source_size = 0;
    std::uint16_t division = 0;
    std::size_t tempo_events = 0;
    double duration_seconds = 0;
    MidiInstrumentPlan guitar;
    MidiInstrumentPlan bass;
    bool ready = false;
};

[[nodiscard]] MidiSessionPlan create_midi_session_plan(
    const runtime::MidiFileSummary& midi,
    std::string source_sha256);
[[nodiscard]] MidiSessionPlan create_midi_session_plan(const std::filesystem::path& midi_path);
[[nodiscard]] std::string format_json(const MidiSessionPlan& plan);

} // namespace rb4pc::builder
