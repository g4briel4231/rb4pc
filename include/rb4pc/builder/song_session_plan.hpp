#pragma once

#include "rb4pc/builder/midi_session_plan.hpp"
#include "rb4pc/runtime/mogg_mix.hpp"
#include "rb4pc/runtime/mogg_parser.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace rb4pc::builder {

struct SongSessionPlan {
    MidiSessionPlan midi;
    std::string mogg_sha256;
    std::string mix_metadata_sha256;
    std::uint32_t mogg_version = 0;
    std::uint32_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint64_t audio_frames = 0;
    double audio_duration_seconds = 0.0;
    std::vector<std::string> stems;
    bool guitar_stem = false;
    bool timeline_fits_audio = false;
    bool ready = false;
};

[[nodiscard]] SongSessionPlan create_song_session_plan(
    MidiSessionPlan midi, const runtime::MoggHeader& mogg,
    const runtime::OggVorbisInfo& vorbis, const runtime::MoggMixMetadata& mix,
    std::string mogg_sha256, std::string mix_metadata_sha256);
[[nodiscard]] SongSessionPlan create_song_session_plan(
    const std::filesystem::path& midi_path, const std::filesystem::path& mogg_path,
    const std::filesystem::path& mix_metadata_path);
[[nodiscard]] std::string format_json(const SongSessionPlan& plan);

} // namespace rb4pc::builder
