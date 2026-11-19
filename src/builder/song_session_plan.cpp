#include "rb4pc/builder/song_session_plan.hpp"

#include "rb4pc/inspector/sha256.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace rb4pc::builder {

SongSessionPlan create_song_session_plan(MidiSessionPlan midi,
                                         const runtime::MoggHeader& mogg,
                                         const runtime::OggVorbisInfo& vorbis,
                                         const runtime::MoggMixMetadata& mix,
                                         std::string mogg_sha256,
                                         std::string mix_metadata_sha256) {
    SongSessionPlan result;
    result.midi = std::move(midi);
    result.mogg_sha256 = std::move(mogg_sha256);
    result.mix_metadata_sha256 = std::move(mix_metadata_sha256);
    result.mogg_version = mogg.version;
    result.channels = vorbis.channels;
    result.sample_rate = vorbis.sample_rate;
    result.audio_frames = vorbis.final_granule;
    result.audio_duration_seconds = vorbis.duration_seconds;
    for (const auto& stem : mix.stems) result.stems.push_back(stem.name);
    result.guitar_stem = std::find(result.stems.begin(), result.stems.end(), "guitar") != result.stems.end();
    result.timeline_fits_audio = result.midi.guitar.expert_chart_end_seconds <=
                                 result.audio_duration_seconds + 0.25;
    result.ready = result.midi.ready && mogg.version == 10 && mogg.embedded_ogg &&
                   result.channels != 0 && result.sample_rate != 0 && result.audio_frames != 0 &&
                   mix.pans.size() == result.channels && mix.volumes_db.size() == result.channels &&
                   result.guitar_stem && result.timeline_fits_audio;
    return result;
}

SongSessionPlan create_song_session_plan(const std::filesystem::path& midi_path,
                                         const std::filesystem::path& mogg_path,
                                         const std::filesystem::path& mix_metadata_path) {
    const auto mogg = runtime::parse_mogg_header(mogg_path);
    const auto vorbis = runtime::inspect_ogg_vorbis(mogg_path, mogg.ogg_offset);
    const auto mix = runtime::parse_mogg_mix_metadata(mix_metadata_path, vorbis.channels);
    return create_song_session_plan(create_midi_session_plan(midi_path), mogg, vorbis, mix,
                                    inspector::sha256_file(mogg_path),
                                    inspector::sha256_file(mix_metadata_path));
}

std::string format_json(const SongSessionPlan& plan) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n  \"midi\": {\"sha256\": \"" << plan.midi.source_sha256
           << "\", \"durationSeconds\": " << plan.midi.duration_seconds
           << ", \"expertGuitarChords\": " << plan.midi.guitar.expert_chords
           << ", \"expertGuitarEndSeconds\": " << plan.midi.guitar.expert_chart_end_seconds << "},\n"
           << "  \"audio\": {\"sha256\": \"" << plan.mogg_sha256
           << "\", \"moggVersion\": " << plan.mogg_version
           << ", \"channels\": " << plan.channels << ", \"sampleRate\": " << plan.sample_rate
           << ", \"frames\": " << plan.audio_frames << ", \"durationSeconds\": "
           << plan.audio_duration_seconds << "},\n  \"mixMetadata\": {\"sha256\": \""
           << plan.mix_metadata_sha256 << "\", \"stems\": [";
    for (std::size_t index = 0; index < plan.stems.size(); ++index) {
        if (index != 0) output << ", ";
        output << '"' << plan.stems[index] << '"';
    }
    output << "]},\n  \"checks\": {\"guitarStem\": " << (plan.guitar_stem ? "true" : "false")
           << ", \"timelineFitsAudio\": " << (plan.timeline_fits_audio ? "true" : "false")
           << "},\n  \"readyForSongSession\": " << (plan.ready ? "true" : "false") << "\n}\n";
    return output.str();
}

} // namespace rb4pc::builder
