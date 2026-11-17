#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/archive_layout.hpp"
#include "rb4pc/inspector/payload_catalog.hpp"
#include "rb4pc/inspector/record_extractor.hpp"
#include "rb4pc/inspector/riff_parser.hpp"

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
    std::filesystem::remove_all(directory);

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
    append_u32(riff_bytes, 12);
    riff_bytes.insert(riff_bytes.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    append_u32(riff_bytes, 0);
    std::stringstream riff_stream(std::string(riff_bytes.begin(), riff_bytes.end()),
                                  std::ios::in | std::ios::binary);
    const auto riff_analysis = rb4pc::inspector::analyze_riff(riff_stream, 0, riff_bytes.size());
    passed &= expect(riff_analysis.structure_valid && riff_analysis.size_matches_record &&
                         riff_analysis.form_type == "WAVE" && riff_analysis.chunk_count == 1 &&
                         riff_analysis.chunk_ids.at("fmt ") == 1,
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

    return passed ? 0 : 1;
}

