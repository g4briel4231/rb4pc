#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/archive_layout.hpp"
#include "rb4pc/builder/build_plan.hpp"
#include "rb4pc/inspector/asset_converter.hpp"
#include "rb4pc/inspector/payload_catalog.hpp"
#include "rb4pc/inspector/record_extractor.hpp"
#include "rb4pc/inspector/riff_parser.hpp"
#include "rb4pc/runtime/archive_file_system.hpp"
#include "rb4pc/runtime/wave_loader.hpp"
#include "rb4pc/runtime/pcm_stream.hpp"
#include "rb4pc/runtime/runtime_session.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
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

} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path() / "rb4pc-inspector-tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directory(directory);
    const auto hash_path = directory / "hash-test.bin";
    const auto path = directory / "main_ps4.hdr";
    const std::string contents = "abc";

    write_file(hash_path, contents);
    write_file(directory / "main_ps4_2.ark", "two");
    write_file(directory / "patch_main_ps4_0.ark", "two");
    write_test_hdr(path);

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
    const auto runtime_session = rb4pc::runtime::RuntimeSession::from_report(conversion_report);
    const auto session_waves = runtime_session.discover_wave_assets("audio/", 5);
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
                         pcm_stream.remaining_frames() == 0 && runtime_session.plan().ready &&
                         runtime_session.plan().wave_records == 1 && session_waves.size() == 1,
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
    passed &= expect(json.find("\"hashIndexValidated\": true") != std::string::npos,
                     "JSON path hash validation missing");

    std::filesystem::remove_all(directory);
    return passed ? 0 : 1;
}

