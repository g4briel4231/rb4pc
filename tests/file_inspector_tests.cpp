#include "rb4pc/inspector/file_inspector.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    append_u32(plain, 0x55667788);
    append_u32(plain, 0x11223344);
    append_string(plain, "test/file.bin");
    append_u32(plain, 7);
    append_u32(plain, 99);

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
                         set_report.metadata.file_records[0].global_offset == 0x1122334455667788ULL &&
                         set_report.metadata.file_records[0].path == "test/file.bin" &&
                         set_report.metadata.file_records[0].unknown_after_path == 7 &&
                         set_report.metadata.file_records[0].size == 99,
                     "file record was not decoded");
    passed &= expect(json.find("\"schemaVersion\": 3") != std::string::npos,
                     "JSON schema version missing");
    passed &= expect(json.find("\"sha256\": \"") != std::string::npos,
                     "JSON hash field missing");
    passed &= expect(json.find("\"method\": \"shannon-byte-frequency\"") != std::string::npos,
                     "JSON analysis method missing");
    passed &= expect(json.find("\"version\": 10") != std::string::npos,
                     "JSON decoded HDR version missing");

    return passed ? 0 : 1;
}

