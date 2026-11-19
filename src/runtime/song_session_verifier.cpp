#include "rb4pc/runtime/song_session_verifier.hpp"

#include "rb4pc/runtime/five_fret_gameplay.hpp"
#include "rb4pc/runtime/five_fret_import.hpp"
#include "rb4pc/runtime/mogg_mix.hpp"
#include "rb4pc/runtime/mogg_parser.hpp"
#include "rb4pc/runtime/vorbis_playback_queue.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace rb4pc::runtime {

SongSessionVerification verify_song_session(const std::filesystem::path& midi_path,
                                             const std::filesystem::path& mogg_path,
                                             const std::filesystem::path& mix_metadata_path,
                                             std::string track_name,
                                             ChartDifficulty difficulty) {
    const auto imported = import_five_fret_midi(midi_path, track_name, difficulty);
    if (imported.chart.chords.empty()) throw std::runtime_error("song session chart has no chords");
    const auto mogg = parse_mogg_header(mogg_path);
    const auto vorbis = inspect_ogg_vorbis(mogg_path, mogg.ogg_offset);
    const auto mix = parse_mogg_mix_metadata(mix_metadata_path, vorbis.channels);
    VorbisPlaybackQueue queue(mogg_path, mogg.ogg_offset);
    FiveFretGameplay gameplay(imported.chart);
    SongSessionVerification result;
    result.track_name = std::move(track_name);
    result.difficulty = difficulty;
    result.chart_chords = imported.chart.chords.size();
    result.finite_pcm = true;
    std::size_t next_autoplay = 0;
    queue.play();

    while (!queue.source_exhausted()) {
        const auto decoded = queue.queue_next(4096);
        if (decoded.frame_count() == 0) throw std::runtime_error("song session audio decoder stalled");
        const auto stereo = mix_mogg_to_stereo(decoded, mix);
        for (const auto sample : stereo.samples) result.finite_pcm = result.finite_pcm && std::isfinite(sample);
        result.stereo_peak = std::max(result.stereo_peak, stereo.peak_amplitude);
        result.clipped_samples += stereo.clipped_samples;
        ++result.decoded_blocks;
        queue.mark_completed(decoded.frame_count());
        const auto current_seconds = queue.transport().seconds();
        while (next_autoplay < imported.chart.chords.size() &&
               imported.chart.chords[next_autoplay].start_seconds <= current_seconds) {
            const auto& chord = imported.chart.chords[next_autoplay];
            const auto hit = gameplay.strum(chord.lane_mask, chord.start_seconds);
            if (!hit || hit->chord_index != next_autoplay)
                throw std::runtime_error("deterministic song-session autoplay lost synchronization");
            ++next_autoplay;
        }
        gameplay.update(current_seconds);
    }
    result.audio_frames = queue.transport().completed_frames();
    result.audio_seconds = queue.transport().seconds();
    result.autoplay_hits = gameplay.hit_count();
    result.autoplay_misses = gameplay.miss_count();
    result.score = gameplay.score();
    result.transport_finished = queue.transport().finished();
    result.gameplay_finished = gameplay.finished();
    result.passed = result.finite_pcm && result.transport_finished && result.gameplay_finished &&
                    result.autoplay_hits == result.chart_chords && result.autoplay_misses == 0;
    return result;
}

} // namespace rb4pc::runtime
