#pragma once

#include "rb4pc/runtime/five_fret_chart.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace rb4pc::runtime {

struct SongSessionVerification {
    std::string track_name;
    ChartDifficulty difficulty = ChartDifficulty::expert;
    std::uint64_t audio_frames = 0;
    double audio_seconds = 0.0;
    std::size_t decoded_blocks = 0;
    std::size_t chart_chords = 0;
    std::size_t autoplay_hits = 0;
    std::size_t autoplay_misses = 0;
    std::uint64_t score = 0;
    float stereo_peak = 0.0F;
    std::uint64_t clipped_samples = 0;
    bool finite_pcm = false;
    bool transport_finished = false;
    bool gameplay_finished = false;
    bool passed = false;
};

[[nodiscard]] SongSessionVerification verify_song_session(
    const std::filesystem::path& midi_path, const std::filesystem::path& mogg_path,
    const std::filesystem::path& mix_metadata_path,
    std::string track_name = "PART GUITAR",
    ChartDifficulty difficulty = ChartDifficulty::expert);

} // namespace rb4pc::runtime
