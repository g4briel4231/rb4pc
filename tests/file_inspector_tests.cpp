#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/archive_layout.hpp"
#include "rb4pc/builder/build_plan.hpp"
#include "rb4pc/builder/song_layout.hpp"
#include "rb4pc/builder/custom_song_catalog.hpp"
#include "rb4pc/inspector/asset_converter.hpp"
#include "rb4pc/inspector/payload_catalog.hpp"
#include "rb4pc/inspector/record_extractor.hpp"
#include "rb4pc/inspector/riff_parser.hpp"
#include "rb4pc/inspector/sha256.hpp"
#include "rb4pc/inspector/texture_container.hpp"
#include "rb4pc/runtime/archive_file_system.hpp"
#include "rb4pc/runtime/wave_loader.hpp"
#include "rb4pc/runtime/pcm_stream.hpp"
#include "rb4pc/runtime/runtime_session.hpp"
#include "rb4pc/runtime/input.hpp"
#include "rb4pc/runtime/window.hpp"
#include "rb4pc/runtime/ui.hpp"
#include "rb4pc/runtime/controller_icons.hpp"
#include "rb4pc/runtime/gameplay_timing.hpp"
#include "rb4pc/runtime/shell.hpp"
#include "rb4pc/runtime/shell_render.hpp"
#include "rb4pc/runtime/midi_parser.hpp"
#include "rb4pc/runtime/rbmid_parser.hpp"
#include "rb4pc/runtime/five_fret_import.hpp"
#include "rb4pc/runtime/mogg_parser.hpp"
#include "rb4pc/runtime/mogg_mix.hpp"
#include "rb4pc/runtime/mogg_audio_output.hpp"
#include "rb4pc/runtime/song_directory.hpp"
#include "rb4pc/runtime/song_library.hpp"
#include "rb4pc/runtime/song_browser.hpp"
#include "rb4pc/runtime/vorbis_decoder.hpp"
#include "rb4pc/runtime/five_fret_chart.hpp"
#include "rb4pc/runtime/timed_chart.hpp"
#include "rb4pc/runtime/five_fret_gameplay.hpp"
#include "rb4pc/runtime/note_highway.hpp"
#include "rb4pc/runtime/five_fret_input.hpp"
#include "rb4pc/runtime/five_fret_session.hpp"
#include "rb4pc/runtime/audio_transport.hpp"
#include "rb4pc/runtime/pcm_playback_queue.hpp"
#include "rb4pc/builder/midi_session_plan.hpp"
#include "rb4pc/builder/song_session_plan.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

void write_file(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void append_u32(std::vector<unsigned char>& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) output.push_back(static_cast<unsigned char>(value >> shift));
}

void append_u64(std::vector<unsigned char>& output, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) output.push_back(static_cast<unsigned char>(value >> shift));
}

void append_string(std::vector<unsigned char>& output, const std::string& value) {
    append_u32(output, static_cast<std::uint32_t>(value.size()));
    output.insert(output.end(), value.begin(), value.end());
}

void write_test_hdr(const std::filesystem::path& path) {
    std::vector<unsigned char> plain;
    append_u32(plain, 10);
    append_u32(plain, 1);
    for (unsigned char value = 0; value < 16; ++value) plain.push_back(value);
    append_u32(plain, 2);
    append_u32(plain, 2);
    append_u32(plain, 3);
    append_u32(plain, 3);
    append_u32(plain, 2);
    append_string(plain, "main_ps4_2.ark");
    append_string(plain, "patch_main_ps4_0.ark");
    append_u32(plain, 2);
    append_u32(plain, 1);
    append_string(plain, "launch");
    append_u32(plain, 0);
    append_u32(plain, 1);
    append_u32(plain, 3);
    append_u32(plain, 0);
    append_string(plain, "test/file.bin");
    append_u32(plain, 0xffffffff);
    append_u32(plain, 3);
    append_u32(plain, 1);
    append_u32(plain, 0);

    constexpr std::uint32_t seed = 12345;
    std::int64_t state = seed;
    std::vector<unsigned char> encoded;
    append_u32(encoded, seed);
    for (const auto value : plain) {
        const auto quotient = state / 127773;
        auto next = 16807 * (state % 127773) - 2836 * quotient;
        if (next <= 0) next += 2147483647;
        state = next;
        encoded.push_back(value ^ static_cast<unsigned char>(state & 0xff));
    }
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
}

void write_test_texture(const std::filesystem::path& path, std::uint32_t version,
                        std::uint32_t marker, unsigned char payload_delta = 0,
                        std::uint32_t magic = 6) {
    std::vector<unsigned char> bytes;
    append_u32(bytes, magic);
    append_u32(bytes, version);
    const auto header_size = magic == 4 && version == 8 ? 164U : (version == 12 ? 124U : 172U);
    bytes.resize(bytes.size() + header_size, 0);
    append_u32(bytes, 2);
    for (const auto size : {4U, 2U}) {
        append_u32(bytes, size);
        append_u32(bytes, size);
        append_u32(bytes, 1);
        if (version == 12) {
            append_u32(bytes, 0);
            append_u32(bytes, 0);
        }
    }
    append_u32(bytes, marker);
    append_u32(bytes, 8);
    for (unsigned char value = 0; value < 8; ++value) bytes.push_back(value + payload_delta);
    append_u32(bytes, 2);
    bytes.push_back(0xaa + payload_delta);
    bytes.push_back(0x55 + payload_delta);
    bytes.resize(bytes.size() + 28, 0);
    write_file(path, std::string(bytes.begin(), bytes.end()));
}

} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path() / "rb4pc-inspector-tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directory(directory);
    const auto hash_path = directory / "hash-test.bin";
    const auto path = directory / "main_ps4.hdr";
    const std::string contents = "abc";

    const auto texture_directory = directory / "texture-corpus";
    std::filesystem::create_directory(texture_directory);
    const auto ps4_texture_path = texture_directory / "texture-ps4.texture";
    const auto pc_texture_path = texture_directory / "texture-pc.texture";
    const auto changed_texture_path = texture_directory / "texture-changed.texture";
    const auto legacy_texture_path = texture_directory / "texture-legacy.texture";
    write_test_texture(ps4_texture_path, 12, 7);
    write_test_texture(pc_texture_path, 9, 6);
    write_test_texture(changed_texture_path, 9, 6, 1);
    write_test_texture(legacy_texture_path, 8, 6, 0, 4);
    const auto ps4_texture = rb4pc::inspector::inspect_texture_container(ps4_texture_path);
    const auto pc_texture = rb4pc::inspector::inspect_texture_container(pc_texture_path);
    const auto changed_texture = rb4pc::inspector::inspect_texture_container(changed_texture_path);
    const auto legacy_texture = rb4pc::inspector::inspect_texture_container(legacy_texture_path);
    const auto texture_match = rb4pc::inspector::compare_texture_payloads(ps4_texture, pc_texture);
    const auto texture_mismatch = rb4pc::inspector::compare_texture_payloads(ps4_texture, changed_texture);
    const auto texture_corpus = rb4pc::inspector::inspect_texture_corpus(texture_directory, ".texture");

    write_file(hash_path, contents);
    write_file(directory / "main_ps4_2.ark", "two");
    write_file(directory / "patch_main_ps4_0.ark", "two");
    write_test_hdr(path);
    const auto midi_path = directory / "notes.mid";
    write_file(midi_path, std::string("MThd\0\0\0\6\0\1\0\2\x01\xe0", 14));
    const auto track_midi_path = directory / "track.mid";
    const auto rbmid_path = directory / "generated.rbmid_pc";
    const auto mogg_path = directory / "audio.mogg";
    const auto ogg_path = directory / "audio.ogg";
    const auto mogg_mix_path = directory / "audio.mogg.dta";
    const std::array<unsigned char, 50> track_midi = {
        'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 1, 0xe0,
        'M', 'T', 'r', 'k', 0, 0, 0, 28,
        0, 0xff, 3, 4, 'T', 'e', 's', 't',
        0, 0xff, 0x51, 3, 7, 0xa1, 0x20,
        0, 0x90, 0x3c, 0x64,
        0x83, 0x60, 0x80, 0x3c, 0,
        0, 0xff, 0x2f, 0};
    write_file(track_midi_path, std::string(track_midi.begin(), track_midi.end()));
    write_file(rbmid_path, std::string("\x10\0\0\0synthetic-rbmid", 19));
    std::vector<unsigned char> synthetic_mogg;
    append_u32(synthetic_mogg, 10);
    append_u32(synthetic_mogg, 12);
    append_u32(synthetic_mogg, 0);
    synthetic_mogg.insert(synthetic_mogg.end(), {'O', 'g', 'g', 'S', 0, 6});
    append_u64(synthetic_mogg, 48'000);
    append_u32(synthetic_mogg, 1);
    append_u32(synthetic_mogg, 0);
    append_u32(synthetic_mogg, 0);
    synthetic_mogg.push_back(1);
    synthetic_mogg.push_back(30);
    synthetic_mogg.insert(synthetic_mogg.end(), {1, 'v', 'o', 'r', 'b', 'i', 's'});
    append_u32(synthetic_mogg, 0);
    synthetic_mogg.push_back(2);
    append_u32(synthetic_mogg, 48'000);
    append_u32(synthetic_mogg, 0);
    append_u32(synthetic_mogg, 0);
    append_u32(synthetic_mogg, 0);
    synthetic_mogg.push_back(0xb8);
    synthetic_mogg.push_back(1);
    write_file(mogg_path, std::string(synthetic_mogg.begin(), synthetic_mogg.end()));
    write_file(mogg_mix_path,
               "; synthetic mix\n(tracks ((drum (0 1))))\n"
               "(pans (-1.0 1.0))\n(vols (0.0 0.0))\n");
    const auto conventional_song = directory / "songs" / "example";
    std::filesystem::create_directories(conventional_song);
    std::filesystem::copy_file(track_midi_path, conventional_song / "notes.mid");
    std::filesystem::copy_file(mogg_path, conventional_song / "example.mogg");
    std::filesystem::copy_file(mogg_mix_path, conventional_song / "example.mogg.dta");
    write_file(conventional_song / "session.json",
               "{\"hashes\":{\"midi\":\"" +
                   rb4pc::inspector::sha256_file(conventional_song / "notes.mid") +
                   "\",\"mogg\":\"" + rb4pc::inspector::sha256_file(conventional_song / "example.mogg") +
                   "\",\"mixMetadata\":\"" +
                   rb4pc::inspector::sha256_file(conventional_song / "example.mogg.dta") + "\"}}");
    const auto resolved_song = rb4pc::runtime::resolve_song_directory(conventional_song);
    const auto tampered_song = directory / "songs" / "tampered";
    std::filesystem::create_directories(tampered_song);
    std::filesystem::copy_file(conventional_song / "notes.mid", tampered_song / "notes.mid");
    std::filesystem::copy_file(conventional_song / "example.mogg", tampered_song / "tampered.mogg");
    std::filesystem::copy_file(conventional_song / "example.mogg.dta", tampered_song / "tampered.mogg.dta");
    std::filesystem::copy_file(conventional_song / "session.json", tampered_song / "session.json");
    write_file(tampered_song / "notes.mid", "tampered");
    bool tampered_song_rejected = false;
    try { static_cast<void>(rb4pc::runtime::resolve_song_directory(tampered_song)); }
    catch (const std::runtime_error&) { tampered_song_rejected = true; }
    const auto song_library = rb4pc::runtime::scan_song_library(directory / "songs");
    rb4pc::runtime::SongBrowserModel song_browser(song_library);
    song_browser.move(1);
    rb4pc::runtime::SoftwareFramebuffer song_browser_framebuffer(640, 360);
    rb4pc::runtime::render_song_browser(
        song_browser_framebuffer, song_library, song_browser, true);
    std::filesystem::copy_file(path, directory / "example_pc.hdr");

    const auto file_report = rb4pc::inspector::inspect_file(hash_path);
    const auto set_report = rb4pc::inspector::inspect_header_set(path);
    const auto record_preview = rb4pc::inspector::inspect_record(set_report, "test/file.bin");
    const auto missing_preview = rb4pc::inspector::inspect_record(set_report, "missing/file.bin");
    const auto payload_catalog = rb4pc::inspector::catalog_payloads(set_report);
    const auto extraction_path = directory / "extracted.bin";
    const auto extraction = rb4pc::inspector::extract_record(
        set_report, "test/file.bin", extraction_path);
    const auto extracted_report = rb4pc::inspector::inspect_file(extraction_path);
    rb4pc::inspector::SelectiveExtractionOptions selective_options;
    selective_options.prefix = "test/";
    selective_options.extension = ".bin";
    const auto selective_path = directory / "selective";
    const auto selective = rb4pc::inspector::extract_records(
        set_report, selective_options, selective_path);
    const auto selective_report = rb4pc::inspector::inspect_file(selective_path / "test" / "file.bin");
    const auto manifest_contents = read_file(selective.manifest);
    const auto json = rb4pc::inspector::format_json(set_report);
    const auto build_plan = rb4pc::builder::create_build_plan(set_report, payload_catalog);
    const auto build_plan_json = rb4pc::builder::format_json(build_plan);
    const auto package_paths = rb4pc::builder::custom_song_package_paths("example");
    const auto custom_song_catalog = rb4pc::builder::scan_custom_songs(directory);
    const auto midi_header = rb4pc::runtime::parse_standard_midi_header(midi_path);
    const auto track_midi_summary = rb4pc::runtime::parse_standard_midi(track_midi_path);
    const auto imported_five_fret = rb4pc::runtime::import_five_fret_midi(
        track_midi_path, "Test", rb4pc::runtime::ChartDifficulty::easy);
    const auto rbmid_header = rb4pc::runtime::parse_rbmid_header(rbmid_path);
    const auto mogg_header = rb4pc::runtime::parse_mogg_header(mogg_path);
    const auto vorbis_info = rb4pc::runtime::inspect_ogg_vorbis(mogg_path, mogg_header.ogg_offset);
    const auto mogg_mix = rb4pc::runtime::parse_mogg_mix_metadata(mogg_mix_path, 2);
    rb4pc::runtime::VorbisPcmBlock synthetic_multichannel{100, 2, 48'000, {0.25F, -0.5F}};
    const auto stereo_mix = rb4pc::runtime::mix_mogg_to_stereo(synthetic_multichannel, mogg_mix);
    const auto pcm16_mix = rb4pc::runtime::stereo_float_to_pcm16(stereo_mix);
    rb4pc::runtime::MoggMixConfig muted_drums;
    muted_drums.stem_levels["drum"] = 0.0F;
    const auto muted_mix = rb4pc::runtime::mix_mogg_to_stereo(
        synthetic_multichannel, mogg_mix, muted_drums);
    bool incomplete_vorbis_rejected = false;
    try {
        static_cast<void>(rb4pc::runtime::decode_vorbis_preview(mogg_path, mogg_header.ogg_offset, 32));
    } catch (const std::runtime_error&) {
        incomplete_vorbis_rejected = true;
    }
    rb4pc::runtime::extract_mogg_ogg(mogg_path, mogg_header, ogg_path);
    auto session_midi = track_midi_summary;
    session_midi.tracks[0].name = "PART GUITAR";
    session_midi.tracks[0].notes[0].key = 96;
    const auto midi_session_plan = rb4pc::builder::create_midi_session_plan(session_midi, "synthetic-sha256");
    auto session_mix = mogg_mix;
    session_mix.stems[0].name = "guitar";
    rb4pc::runtime::MoggMixConfig reduced_guitar;
    reduced_guitar.stem_levels["guitar"] = 0.25F;
    const auto reduced_guitar_mix = rb4pc::runtime::mix_mogg_to_stereo(
        synthetic_multichannel, session_mix, reduced_guitar);
    const auto song_session_plan = rb4pc::builder::create_song_session_plan(
        midi_session_plan, mogg_header, vorbis_info, session_mix,
        "synthetic-mogg-sha256", "synthetic-mix-sha256");
    const auto easy_five_fret = rb4pc::runtime::build_five_fret_chart(
        track_midi_summary.tracks[0], rb4pc::runtime::ChartDifficulty::easy);
    rb4pc::runtime::MidiTrackSummary chord_track;
    chord_track.notes = {{0, 96, 100, 120, 240}, {0, 98, 100, 120, 360}, {0, 100, 100, 480, 600}};
    const auto expert_five_fret = rb4pc::runtime::build_five_fret_chart(
        chord_track, rb4pc::runtime::ChartDifficulty::expert);
    const rb4pc::runtime::ArchiveFileSystem vfs(set_report);
    const auto vfs_bytes = vfs.read("test/file.bin", 1, 2);
    const auto vfs_paths = vfs.list_paths("test/", 10);
    bool passed = true;
    passed &= expect(file_report.size == contents.size() && file_report.preview_size == contents.size(),
                     "unexpected report size");
    passed &= expect(file_report.sha256 == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                     "SHA-256 mismatch");
    const std::array<unsigned char, 256> uniform = [] {
        std::array<unsigned char, 256> values{};
        for (std::size_t index = 0; index < values.size(); ++index) values[index] = static_cast<unsigned char>(index);
        return values;
    }();
    const std::array<unsigned char, 4> zeros{};
    passed &= expect(rb4pc::inspector::shannon_entropy(uniform.data(), uniform.size()) == 8.0,
                     "uniform entropy mismatch");
    passed &= expect(rb4pc::inspector::shannon_entropy(zeros.data(), zeros.size()) == 0.0,
                     "zero entropy mismatch");

    for (std::size_t index = 0; index < contents.size(); ++index) {
        passed &= expect(std::to_integer<unsigned int>(file_report.preview[index]) ==
                             static_cast<unsigned char>(contents[index]),
                         "preview mismatch at byte " + std::to_string(index));
    }

    passed &= expect(set_report.platform_hint == "ps4", "platform hint mismatch");
    passed &= expect(build_plan.ready && build_plan.archive_count == 2 &&
                         build_plan.usable_size_archives == 2 && build_plan.resolved_records == 1 &&
                         build_plan_json.find("\"readyForConversionPlanning\": true") != std::string::npos,
                     "builder plan validation mismatch");
    passed &= expect(rb4pc::builder::is_internal_song_path("ps4/songs/example/song.mogg") &&
                         rb4pc::builder::is_internal_song_path("pc/songs_download/example/song.mogg") &&
                         package_paths.header.generic_string() == "example_pc.hdr" &&
                         package_paths.archive.generic_string() == "example_pc_0.ark",
                     "custom-song package layout mismatch");
    passed &= expect(custom_song_catalog.songs.size() == 1 &&
                         custom_song_catalog.songs[0].short_name == "example" &&
                         !custom_song_catalog.songs[0].ready &&
                         !custom_song_catalog.songs[0].core_content_complete,
                     "custom-song installation scan mismatch");
    passed &= expect(midi_header.format == 1 && midi_header.track_count == 2 && midi_header.division == 480,
                     "standard MIDI header parse mismatch");
    passed &= expect(track_midi_summary.tracks.size() == 1 && track_midi_summary.tracks[0].name == "Test" &&
                         track_midi_summary.tracks[0].note_on_count == 1 &&
                         track_midi_summary.tracks[0].note_off_count == 1 &&
                         track_midi_summary.tracks[0].notes.size() == 1 &&
                         track_midi_summary.tracks[0].notes[0].channel == 0 &&
                         track_midi_summary.tracks[0].notes[0].key == 60 &&
                         track_midi_summary.tracks[0].notes[0].velocity == 100 &&
                         track_midi_summary.tracks[0].notes[0].start_tick == 0 &&
                         track_midi_summary.tracks[0].notes[0].end_tick == 480 &&
                         track_midi_summary.tracks[0].unmatched_note_off_count == 0 &&
                         track_midi_summary.tracks[0].dangling_note_on_count == 0 &&
                         track_midi_summary.tempo_changes.size() == 1 &&
                         track_midi_summary.tempo_changes[0].microseconds_per_quarter == 500000 &&
                         rb4pc::runtime::midi_tick_to_seconds(track_midi_summary, 480) == 0.5 &&
                         track_midi_summary.tracks[0].end_tick == 480,
                     "standard MIDI track parse mismatch");
    passed &= expect(midi_session_plan.ready && midi_session_plan.guitar.easy_chords == 0 &&
                         midi_session_plan.guitar.expert_chords == 1 &&
                         midi_session_plan.duration_seconds == 0.5 &&
                         rb4pc::builder::format_json(midi_session_plan).find("\"readyForFiveFretSession\": true") != std::string::npos,
                     "MIDI session-plan mismatch");
    passed &= expect(song_session_plan.ready && song_session_plan.guitar_stem &&
                         song_session_plan.timeline_fits_audio && song_session_plan.channels == 2 &&
                         song_session_plan.audio_frames == 48'000 &&
                         rb4pc::builder::format_json(song_session_plan).find(
                             "\"readyForSongSession\": true") != std::string::npos,
                     "complete song-session plan mismatch");
    passed &= expect(rbmid_header.format == rb4pc::runtime::RBMidFormat::rb4 && rbmid_header.file_size == 19,
                     "RBMID envelope parse mismatch");
    passed &= expect(imported_five_fret.track_name == "Test" && imported_five_fret.chart.chords.size() == 1 &&
                         imported_five_fret.chart.chords[0].start_seconds == 0.0 &&
                         imported_five_fret.chart.chords[0].end_seconds == 0.5,
                     "five-fret MIDI import mismatch");
    passed &= expect(mogg_header.version == 10 && mogg_header.ogg_offset == 12 && mogg_header.embedded_ogg &&
                         read_file(ogg_path).size() == synthetic_mogg.size() - 12 &&
                         vorbis_info.channels == 2 && vorbis_info.sample_rate == 48'000 &&
                         vorbis_info.page_count == 1 && vorbis_info.final_granule == 48'000 &&
                         vorbis_info.duration_seconds == 1.0 && incomplete_vorbis_rejected,
                     "MOGG embedded-Ogg extraction mismatch");
    passed &= expect(mogg_mix.stems.size() == 1 && mogg_mix.stems[0].name == "drum" &&
                         mogg_mix.stems[0].channels == std::vector<std::uint32_t>({0, 1}) &&
                         stereo_mix.first_frame == 100 && stereo_mix.sample_rate == 48'000 &&
                         stereo_mix.frame_count() == 1 && stereo_mix.samples[0] == 0.25F &&
                         stereo_mix.samples[1] == -0.5F && stereo_mix.clipped_samples == 0 &&
                         pcm16_mix == std::vector<std::int16_t>({8192, -16384}) &&
                         muted_mix.peak_amplitude == 0.0F &&
                         reduced_guitar_mix.samples[0] == 0.0625F &&
                         reduced_guitar_mix.samples[1] == -0.125F,
                     "MOGG metadata stereo-mix mismatch");
    passed &= expect(resolved_song.midi.filename() == "notes.mid" &&
                         resolved_song.mogg.filename() == "example.mogg" &&
                         resolved_song.mix_metadata.filename() == "example.mogg.dta" &&
                         resolved_song.integrity_verified && tampered_song_rejected,
                     "conventional song-directory resolution mismatch");
    passed &= expect(tampered_song_rejected && song_library.entries.size() == 2 &&
                         song_library.valid_entries == 1 &&
                         song_library.entries[0].short_name == "example" &&
                         song_library.entries[0].valid &&
                         song_library.entries[1].short_name == "tampered" &&
                         !song_library.entries[1].valid && !song_library.entries[1].error.empty() &&
                         song_browser.valid_count() == 1 &&
                         song_browser.selected().short_name == "example" &&
                         song_browser_framebuffer.pixels().size() == 640U * 360U,
                     "song-library integrity scan mismatch");
    passed &= expect(easy_five_fret.chords.size() == 1 && easy_five_fret.chords[0].start_tick == 0 &&
                         easy_five_fret.chords[0].end_tick == 480 && easy_five_fret.chords[0].lane_mask == 1,
                     "five-fret chart mapping mismatch");
    passed &= expect(expert_five_fret.chords.size() == 2 && expert_five_fret.chords[0].start_tick == 120 &&
                         expert_five_fret.chords[0].end_tick == 360 && expert_five_fret.chords[0].lane_mask == 5 &&
                         expert_five_fret.chords[1].lane_mask == 16,
                     "five-fret chord grouping mismatch");
    rb4pc::runtime::MidiFileSummary changing_tempo;
    changing_tempo.header.division = 480;
    changing_tempo.tempo_changes = {{480, 1'000'000}};
    passed &= expect(rb4pc::runtime::midi_tick_to_seconds(changing_tempo, 960) == 1.5,
                     "MIDI changing-tempo timeline mismatch");
    const auto timed_five_fret = rb4pc::runtime::time_five_fret_chart(changing_tempo, expert_five_fret);
    const auto visible_chords = rb4pc::runtime::visible_five_fret_chords(timed_five_fret, 0.5, 0.13, 0.25);
    passed &= expect(timed_five_fret.chords.size() == 2 && timed_five_fret.chords[0].start_seconds == 0.125 &&
                         timed_five_fret.chords[0].end_seconds == 0.375 &&
                         timed_five_fret.chords[1].start_seconds == 0.5 &&
                         timed_five_fret.chords[1].end_seconds == 0.75 &&
                         visible_chords.size() == 2 && visible_chords[0] == 0 && visible_chords[1] == 1,
                     "timed five-fret chart window mismatch");
    rb4pc::runtime::FiveFretGameplay five_fret_gameplay(timed_five_fret);
    five_fret_gameplay.update(0.3);
    const auto chord_hit = five_fret_gameplay.strum(16, 0.52);
    passed &= expect(chord_hit && chord_hit->chord_index == 1 &&
                         chord_hit->timing.judgement == rb4pc::runtime::Judgement::perfect &&
                         five_fret_gameplay.hit_count() == 1 && five_fret_gameplay.miss_count() == 1 &&
                         five_fret_gameplay.score() == 100 && five_fret_gameplay.finished() &&
                         five_fret_gameplay.states()[0] == rb4pc::runtime::ChordState::missed &&
                         five_fret_gameplay.states()[1] == rb4pc::runtime::ChordState::hit,
                     "five-fret gameplay lifecycle mismatch");
    rb4pc::runtime::SoftwareFramebuffer highway_framebuffer(320, 200);
    const auto highway_stats = rb4pc::runtime::render_note_highway(
        highway_framebuffer, timed_five_fret, {0.5, 0.13, 0.25});
    passed &= expect(highway_stats.visible_chords == 2 && highway_stats.rendered_gems == 3 &&
                         highway_stats.rendered_sustains == 3 &&
                         highway_framebuffer.pixels().size() == 320U * 200U,
                     "note-highway rendering mismatch");
    rb4pc::runtime::SoftwareFramebuffer controller_icons(416, 64);
    controller_icons.clear(0);
    const std::array prompts{
        rb4pc::runtime::ControllerPrompt::a, rb4pc::runtime::ControllerPrompt::b,
        rb4pc::runtime::ControllerPrompt::x, rb4pc::runtime::ControllerPrompt::y,
        rb4pc::runtime::ControllerPrompt::dpad, rb4pc::runtime::ControllerPrompt::left_bumper,
        rb4pc::runtime::ControllerPrompt::right_bumper, rb4pc::runtime::ControllerPrompt::left_trigger,
        rb4pc::runtime::ControllerPrompt::right_trigger, rb4pc::runtime::ControllerPrompt::left_stick,
        rb4pc::runtime::ControllerPrompt::right_stick, rb4pc::runtime::ControllerPrompt::menu,
        rb4pc::runtime::ControllerPrompt::view};
    for (std::size_t index = 0; index < prompts.size(); ++index)
        rb4pc::runtime::render_controller_icon(
            controller_icons, prompts[index], {static_cast<std::uint32_t>(index * 32), 0, 30, 30});
    const auto green_pixels = std::count(controller_icons.pixels().begin(), controller_icons.pixels().end(), 0xff3aa655U);
    const auto red_pixels = std::count(controller_icons.pixels().begin(), controller_icons.pixels().end(), 0xffd64a45U);
    const auto blue_pixels = std::count(controller_icons.pixels().begin(), controller_icons.pixels().end(), 0xff318ac7U);
    const auto yellow_pixels = std::count(controller_icons.pixels().begin(), controller_icons.pixels().end(), 0xffe2b93bU);
    passed &= expect(rb4pc::runtime::default_controller_icon_theme() == rb4pc::runtime::ControllerIconTheme::xbox &&
                         rb4pc::runtime::controller_prompt_for_action(rb4pc::runtime::ControllerAction::confirm) ==
                             rb4pc::runtime::ControllerPrompt::a &&
                         rb4pc::runtime::controller_prompt_for_action(rb4pc::runtime::ControllerAction::cancel) ==
                             rb4pc::runtime::ControllerPrompt::b &&
                         green_pixels > 0 && red_pixels > 0 && blue_pixels > 0 && yellow_pixels > 0,
                     "default Xbox controller-icon rendering mismatch");
    rb4pc::runtime::FiveFretInputMapper input_mapper;
    rb4pc::runtime::ControllerState mapped_controller;
    mapped_controller.connected = true; mapped_controller.buttons = 0x1000 | 0x2000 | 0x0001;
    const auto first_input = input_mapper.sample(mapped_controller);
    const auto held_input = input_mapper.sample(mapped_controller);
    mapped_controller.buttons = 0;
    (void)input_mapper.sample(mapped_controller);
    mapped_controller.buttons = 0x0200 | 0x0002 | 0x0010;
    const auto next_input = input_mapper.sample(mapped_controller);
    auto no_pause_profile = rb4pc::runtime::default_xinput_five_fret_profile();
    no_pause_profile.pause_button = 0;
    rb4pc::runtime::FiveFretInputMapper no_pause_mapper(no_pause_profile);
    rb4pc::runtime::ControllerState menu_only_controller;
    menu_only_controller.connected = true;
    menu_only_controller.buttons = 0x0010;
    const auto no_pause_input = no_pause_mapper.sample(menu_only_controller);
    std::vector<rb4pc::runtime::ControllerState> selection_states(4);
    for (std::uint32_t index = 0; index < selection_states.size(); ++index)
        selection_states[index].index = index;
    selection_states[1].connected = true;
    selection_states[3].connected = true;
    const auto first_connected = rb4pc::runtime::select_connected_controller(selection_states);
    const auto preferred_connected = rb4pc::runtime::select_connected_controller(selection_states, 3);
    const auto disconnected_fallback = rb4pc::runtime::select_connected_controller(selection_states, 2);
    passed &= expect(first_input.connected && first_input.lane_mask == 3 && first_input.strum_pressed &&
                         !first_input.pause_pressed && held_input.lane_mask == 3 && !held_input.strum_pressed &&
                         next_input.lane_mask == 16 && next_input.strum_pressed && next_input.pause_pressed &&
                         !no_pause_input.pause_pressed && first_connected && first_connected->index == 1 &&
                         preferred_connected && preferred_connected->index == 3 &&
                         disconnected_fallback && disconnected_fallback->index == 1,
                     "five-fret input mapping mismatch");
    rb4pc::runtime::FiveFretSession five_fret_session(timed_five_fret);
    rb4pc::runtime::ControllerState session_controller;
    session_controller.connected = true;
    (void)five_fret_session.step(0.48, session_controller);
    session_controller.buttons = 0x0200 | 0x0001;
    const auto session_hit = five_fret_session.step(0.02, session_controller);
    session_controller.buttons = 0;
    (void)five_fret_session.step(0, session_controller);
    session_controller.buttons = 0x0010;
    const auto paused_frame = five_fret_session.step(0.0, session_controller);
    session_controller.buttons = 0;
    (void)five_fret_session.step(0.0, session_controller);
    session_controller.buttons = 0x0010;
    const auto resumed_frame = five_fret_session.step(0.0, session_controller);
    session_controller.buttons = 0;
    const auto after_resume_frame = five_fret_session.step(0.25, session_controller);
    rb4pc::runtime::SoftwareFramebuffer session_framebuffer(320, 200);
    const auto session_render = five_fret_session.render(session_framebuffer, 0.13, 0.25);
    passed &= expect(session_hit.hit && session_hit.hit->chord_index == 1 && session_hit.gameplay_seconds == 0.5 &&
                         five_fret_session.gameplay().hit_count() == 1 && five_fret_session.gameplay().miss_count() == 1 &&
                         paused_frame.paused && paused_frame.gameplay_seconds == 0.5 && !resumed_frame.paused &&
                         after_resume_frame.gameplay_seconds == 0.75 && !five_fret_session.paused() &&
                         session_render.visible_chords == 1,
                     "integrated five-fret session mismatch");
    rb4pc::runtime::AudioTransportClock audio_transport(48000, 96000);
    audio_transport.play(); audio_transport.complete_frames(24000);
    rb4pc::runtime::FiveFretSession synchronized_session(timed_five_fret);
    rb4pc::runtime::ControllerState synchronized_controller;
    synchronized_controller.connected = true; synchronized_controller.buttons = 0x0200 | 0x0001;
    const auto synchronized_hit = synchronized_session.step_to(audio_transport.seconds(), synchronized_controller);
    audio_transport.pause(); audio_transport.complete_frames(24000);
    passed &= expect(audio_transport.completed_frames() == 24000 && audio_transport.seconds() == 0.5 &&
                         !audio_transport.playing() && synchronized_hit.hit &&
                         synchronized_hit.hit->chord_index == 1 && synchronized_session.seconds() == 0.5,
                     "audio-transport gameplay synchronization mismatch");
    passed &= expect(vfs.contains("test/file.bin") && !vfs.contains("missing.bin") &&
                         vfs.file_size("test/file.bin") == 3 && vfs.file_count() == 1 &&
                         vfs_bytes.size() == 2 && std::to_integer<unsigned int>(vfs_bytes[0]) == 'w' &&
                         std::to_integer<unsigned int>(vfs_bytes[1]) == 'o' &&
                         vfs_paths.size() == 1 && vfs_paths[0] == "test/file.bin",
                     "runtime VFS read mismatch");
    passed &= expect(set_report.archives.size() == 2, "archive count mismatch");
    passed &= expect(set_report.metadata.version == 10 && set_report.metadata.unknown_04 == 1,
                     "decoded HDR fields mismatch");
    passed &= expect(set_report.archives.size() == 2 && set_report.archives[0].size_matches &&
                         set_report.archives[1].size_matches,
                     "declared archives were not verified");
    passed &= expect(set_report.archives.size() == 2 &&
                         set_report.archives[1].path.filename() == "patch_main_ps4_0.ark",
                     "patch archive declaration missing");
    passed &= expect(set_report.metadata.archives[0].tags.size() == 1 &&
                         set_report.metadata.archives[0].tags[0] == "launch",
                     "archive tags were not decoded");
    passed &= expect(set_report.metadata.file_records.size() == 1 &&
                         set_report.metadata.file_records[0].global_offset == 3 &&
                         set_report.metadata.file_records[0].path == "test/file.bin" &&
                         set_report.metadata.file_records[0].previous_hash_index == 0xffffffff &&
                         set_report.metadata.file_records[0].size == 3,
                     "file record was not decoded");
    passed &= expect(set_report.metadata.hash_index_validated,
                     "path hash index was not validated");
    passed &= expect(set_report.record_locations.size() == 1 && set_report.record_locations[0] &&
                         set_report.record_locations[0]->archive_index == 1 &&
                         set_report.record_locations[0]->local_offset == 0,
                     "file record archive location was not resolved");
    passed &= expect(record_preview && record_preview->size == 3 &&
                         std::to_integer<unsigned int>(record_preview->bytes[0]) == 't' &&
                         std::to_integer<unsigned int>(record_preview->bytes[1]) == 'w' &&
                         std::to_integer<unsigned int>(record_preview->bytes[2]) == 'o',
                     "file record preview mismatch");
    passed &= expect(!missing_preview, "missing internal path unexpectedly matched");
    passed &= expect(payload_catalog.total_records == 1 && payload_catalog.analyzed_records == 1 &&
                         payload_catalog.empty_records == 0 && payload_catalog.unreadable_records == 0 &&
                         payload_catalog.signatures.at("unknown") == 1 &&
                         payload_catalog.extensions.at(".bin") == 1,
                     "payload catalog mismatch");
    passed &= expect(extraction.size == 3 && extraction.destination == extraction_path &&
                         extraction.sha256 ==
                             "3fc4ccfe745870e2c0d99f71f30ff0656c8dedd41cc1d7d3d376b0dbe685e2f3" &&
                         extracted_report.size == 3 && extracted_report.sha256 == extraction.sha256,
                     "individual extraction mismatch");
    passed &= expect(selective.file_count == 1 && selective.total_size == 3 &&
                         selective_report.sha256 == extraction.sha256 &&
                         manifest_contents.find("\"internalPath\": \"test/file.bin\"") != std::string::npos &&
                         manifest_contents.find(extraction.sha256) != std::string::npos,
                     "selective extraction or manifest mismatch");
    const std::array<unsigned char, 8> png_signature = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    passed &= expect(rb4pc::inspector::classify_payload_signature(
                         png_signature.data(), png_signature.size()) == "png",
                     "PNG signature was not classified");
    std::vector<unsigned char> riff_bytes = {'R', 'I', 'F', 'F'};
    append_u32(riff_bytes, 40);
    riff_bytes.insert(riff_bytes.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    append_u32(riff_bytes, 16);
    riff_bytes.insert(riff_bytes.end(), {1, 0, 2, 0});
    append_u32(riff_bytes, 48000);
    append_u32(riff_bytes, 192000);
    riff_bytes.insert(riff_bytes.end(), {4, 0, 16, 0});
    riff_bytes.insert(riff_bytes.end(), {'d', 'a', 't', 'a'});
    append_u32(riff_bytes, 4);
    riff_bytes.insert(riff_bytes.end(), {0x11, 0x22, 0x33, 0x44});
    std::stringstream riff_stream(std::string(riff_bytes.begin(), riff_bytes.end()),
                                  std::ios::in | std::ios::binary);
    const auto riff_analysis = rb4pc::inspector::analyze_riff(riff_stream, 0, riff_bytes.size());
    passed &= expect(riff_analysis.structure_valid && riff_analysis.size_matches_record &&
                         riff_analysis.form_type == "WAVE" && riff_analysis.chunk_count == 2 &&
                         riff_analysis.chunk_ids.at("fmt ") == 1 && riff_analysis.wave_format &&
                         riff_analysis.wave_format->format_tag == 1 &&
                         riff_analysis.wave_format->channels == 2 &&
                         riff_analysis.wave_format->sample_rate == 48000 &&
                         riff_analysis.wave_format->bits_per_sample == 16,
                     "RIFF structure analysis mismatch");
    auto padded_riff_bytes = riff_bytes;
    padded_riff_bytes.insert(padded_riff_bytes.end(), 4, 0);
    std::stringstream padded_riff_stream(std::string(padded_riff_bytes.begin(), padded_riff_bytes.end()),
                                         std::ios::in | std::ios::binary);
    const auto padded_riff_analysis = rb4pc::inspector::analyze_riff(
        padded_riff_stream, 0, padded_riff_bytes.size());
    passed &= expect(padded_riff_analysis.structure_valid && !padded_riff_analysis.size_matches_record &&
                         padded_riff_analysis.trailing_bytes_size == 4 &&
                         padded_riff_analysis.trailing_bytes_zero,
                     "RIFF zero trailing-region analysis mismatch");
    std::vector<unsigned char> fev_bytes = {'R', 'I', 'F', 'F'};
    append_u32(fev_bytes, 20);
    fev_bytes.insert(fev_bytes.end(), {'F', 'E', 'V', ' ', 'F', 'M', 'T', ' '});
    append_u32(fev_bytes, 8);
    append_u32(fev_bytes, 80);
    append_u32(fev_bytes, 70);
    std::stringstream fev_stream(std::string(fev_bytes.begin(), fev_bytes.end()),
                                 std::ios::in | std::ios::binary);
    const auto fev_analysis = rb4pc::inspector::analyze_riff(fev_stream, 0, fev_bytes.size());
    passed &= expect(fev_analysis.structure_valid && fev_analysis.fev_format_prefix &&
                         fev_analysis.fev_format_prefix->value_0 == 80 &&
                         fev_analysis.fev_format_prefix->value_4 == 70,
                     "FEV FMT prefix analysis mismatch");
    auto conversion_source = riff_bytes;
    conversion_source.insert(conversion_source.end(), {1, 2, 3, 4});
    const auto conversion_ark = directory / "conversion.ark";
    write_file(conversion_ark, std::string(conversion_source.begin(), conversion_source.end()));
    rb4pc::inspector::HeaderSetReport conversion_report;
    conversion_report.metadata.archives.push_back(
        {"conversion.ark", static_cast<std::uint32_t>(conversion_source.size()), {}});
    conversion_report.metadata.file_records.push_back(
        {0, "audio/test.wav", 0xffffffff, static_cast<std::uint32_t>(conversion_source.size())});
    conversion_report.metadata.hash_bucket_heads.push_back(0);
    conversion_report.metadata.version = 10;
    conversion_report.metadata.hash_index_validated = true;
    conversion_report.archives.push_back(
        {conversion_ark, static_cast<std::uint32_t>(conversion_source.size()), conversion_source.size(), true, true});
    conversion_report.record_locations.push_back(rb4pc::inspector::ArchiveLocation{0, 0});
    const rb4pc::runtime::ArchiveFileSystem wave_vfs(conversion_report);
    const auto wave_asset = rb4pc::runtime::load_wave(wave_vfs, "audio/test.wav");
    const auto pcm_bytes = rb4pc::runtime::read_pcm(wave_vfs, wave_asset, 1, 2);
    rb4pc::runtime::PcmStream pcm_stream(wave_vfs, wave_asset);
    const auto pcm_frame = pcm_stream.read_frames(1);
    rb4pc::runtime::PcmPlaybackQueue pcm_queue(wave_vfs, wave_asset);
    pcm_queue.play();
    const auto queued_pcm = pcm_queue.queue_next(8);
    pcm_queue.mark_completed(queued_pcm.frame_count);
    bool excessive_completion_rejected = false;
    try { pcm_queue.mark_completed(1); } catch (const std::runtime_error&) { excessive_completion_rejected = true; }
    const auto runtime_session = rb4pc::runtime::RuntimeSession::from_report(conversion_report);
    const auto session_waves = runtime_session.discover_wave_assets("audio/", 5);
    const auto session_diagnostics = runtime_session.diagnostics_json();
    const auto controller_states = rb4pc::runtime::poll_controllers();
    rb4pc::runtime::SoftwareFramebuffer framebuffer(2, 2);
    framebuffer.clear(0xff102030);
    framebuffer.set_pixel(1, 1, 0xffabcdef);
    rb4pc::runtime::MenuModel menu({"Play", "Settings", "Quit"});
    menu.move(-1);
    rb4pc::runtime::GameplayClock gameplay_clock;
    gameplay_clock.advance(1.0); gameplay_clock.set_paused(true); gameplay_clock.advance(1.0);
    const auto judgement = rb4pc::runtime::judge_note(1.0, 1.025);
    rb4pc::runtime::RuntimeShell shell;
    shell.source_ready(true); shell.dispatch(rb4pc::runtime::ShellAction::select);
    rb4pc::runtime::SoftwareFramebuffer shell_framebuffer(320, 200);
    rb4pc::runtime::RuntimeShell render_shell_model; render_shell_model.source_ready(true);
    rb4pc::runtime::render_shell(shell_framebuffer, render_shell_model, {true, 100, 10});
    const auto converted_path = directory / "converted.wav";
    const auto conversion = rb4pc::inspector::convert_riff_record(
        conversion_report, "audio/test.wav", converted_path);
    const auto converted_report = rb4pc::inspector::inspect_file(converted_path);
    const auto sidecar_report = rb4pc::inspector::inspect_file(*conversion.trailing_sidecar);
    const auto conversion_manifest = read_file(conversion.manifest);
    passed &= expect(conversion.record_size == conversion_source.size() &&
                         conversion.logical_size == riff_bytes.size() &&
                         converted_report.size == riff_bytes.size() && sidecar_report.size == 4 &&
                         conversion.trailing_sha256 == sidecar_report.sha256 && conversion.form_type == "WAVE" &&
                         conversion_manifest.find("\"formType\": \"WAVE\"") != std::string::npos &&
                         conversion_manifest.find(conversion.sha256) != std::string::npos,
                     "format-aware RIFF conversion mismatch");
    passed &= expect(wave_asset.channels == 2 && wave_asset.sample_rate == 48000 &&
                         wave_asset.bits_per_sample == 16 && wave_asset.data_size == 4 &&
                         wave_asset.frame_count == 1 && pcm_bytes.size() == 2 &&
                         std::to_integer<unsigned int>(pcm_bytes[0]) == 0x22 &&
                         std::to_integer<unsigned int>(pcm_bytes[1]) == 0x33 &&
                         pcm_frame.size() == 4 && pcm_stream.frame_position() == 1 &&
                         pcm_stream.remaining_frames() == 0 && queued_pcm.first_frame == 0 &&
                         queued_pcm.frame_count == 1 && queued_pcm.bytes.size() == 4 &&
                         pcm_queue.submitted_frames() == 1 && pcm_queue.outstanding_frames() == 0 &&
                         pcm_queue.source_exhausted() && pcm_queue.transport().finished() && excessive_completion_rejected &&
                         runtime_session.plan().ready &&
                         runtime_session.plan().wave_records == 1 && session_waves.size() == 1 &&
                         session_diagnostics.find("\"gameplay\": false") != std::string::npos &&
                         (!rb4pc::runtime::input_backend_available() || controller_states.size() == 4) &&
                         framebuffer.pixels().size() == 4 && framebuffer.pixels()[0] == 0xff102030 &&
                         framebuffer.pixels()[3] == 0xffabcdef && menu.selected_item() == "Quit" &&
                         gameplay_clock.seconds() == 1.0 &&
                         judgement.judgement == rb4pc::runtime::Judgement::perfect && judgement.score == 100 &&
                         shell.screen() == rb4pc::runtime::ShellScreen::song_browser &&
                         shell_framebuffer.pixels()[0] == 0xff184f36,
                     "runtime WAVE loader mismatch");
    const std::vector<rb4pc::inspector::HdrArchiveEntry> layout = {
        {"first.ark", 3, {}}, {"second.ark", 5, {}}};
    const auto first_location = rb4pc::inspector::resolve_archive_location(layout, 2, 1);
    const auto boundary_location = rb4pc::inspector::resolve_archive_location(layout, 3, 4);
    const auto crossing_location = rb4pc::inspector::resolve_archive_location(layout, 2, 2);
    const auto final_marker_location = rb4pc::inspector::resolve_archive_location(layout, 8, 0);
    passed &= expect(first_location && first_location->archive_index == 0 && first_location->local_offset == 2,
                     "first archive location mismatch");
    passed &= expect(boundary_location && boundary_location->archive_index == 1 &&
                         boundary_location->local_offset == 0,
                     "archive boundary location mismatch");
    passed &= expect(!crossing_location, "cross-volume record unexpectedly resolved");
    passed &= expect(final_marker_location && final_marker_location->archive_index == 1 &&
                         final_marker_location->local_offset == 5,
                     "final marker location mismatch");
    passed &= expect(json.find("\"schemaVersion\": 5") != std::string::npos,
                     "JSON schema version missing");
    passed &= expect(json.find("\"sha256\": \"") != std::string::npos,
                     "JSON hash field missing");
    passed &= expect(json.find("\"method\": \"shannon-byte-frequency\"") != std::string::npos,
                     "JSON analysis method missing");
    passed &= expect(json.find("\"version\": 10") != std::string::npos,
                     "JSON decoded HDR version missing");
    passed &= expect(ps4_texture.platform_version == 12 && ps4_texture.marker == 7 &&
                         ps4_texture.header_data_size == 124 && ps4_texture.mipmap_entry_size == 20 &&
                         pc_texture.platform_version == 9 && pc_texture.marker == 6 &&
                         pc_texture.header_data_size == 172 && pc_texture.mipmap_entry_size == 12 &&
                         legacy_texture.magic == 4 && legacy_texture.header_data_size == 164 &&
                         ps4_texture.mipmaps.size() == 2 && ps4_texture.mipmaps[0].data_size == 8 &&
                         texture_match.compatible && !texture_mismatch.compatible &&
                         texture_corpus.candidate_count == 4 && texture_corpus.parsed_count == 4 &&
                         texture_corpus.families.size() == 3 && texture_corpus.rejected_signatures.empty(),
                     "texture container compatibility mismatch");
    passed &= expect(json.find("\"hashIndexValidated\": true") != std::string::npos,
                     "JSON path hash validation missing");

    std::filesystem::remove_all(directory);
    return passed ? 0 : 1;
}

