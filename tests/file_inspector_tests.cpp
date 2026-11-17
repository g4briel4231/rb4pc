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

} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path() / "rb4pc-inspector-tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directory(directory);
    const auto path = directory / "main_ps4.hdr";
    const std::string contents = "abc";

    write_file(path, contents);
    write_file(directory / "main_ps4_10.ark", "ten");
    write_file(directory / "main_ps4_2.ark", "two");
    write_file(directory / "patch_main_ps4_0.ark", "not associated");

    const auto file_report = rb4pc::inspector::inspect_file(path);
    const auto set_report = rb4pc::inspector::inspect_header_set(path);
    const auto json = rb4pc::inspector::format_json(set_report);
    std::filesystem::remove_all(directory);

    bool passed = true;
    passed &= expect(file_report.size == contents.size() && file_report.preview_size == contents.size(),
                     "unexpected report size");
    passed &= expect(file_report.sha256 == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                     "SHA-256 mismatch");

    for (std::size_t index = 0; index < contents.size(); ++index) {
        passed &= expect(std::to_integer<unsigned int>(file_report.preview[index]) ==
                             static_cast<unsigned char>(contents[index]),
                         "preview mismatch at byte " + std::to_string(index));
    }

    passed &= expect(set_report.platform_hint == "ps4", "platform hint mismatch");
    passed &= expect(set_report.archives.size() == 2, "archive count mismatch");
    passed &= expect(set_report.archives.size() == 2 && set_report.archives[0].index == 2 &&
                         set_report.archives[1].index == 10,
                     "archives were not sorted numerically");
    passed &= expect(json.find("\"schemaVersion\": 1") != std::string::npos,
                     "JSON schema version missing");
    passed &= expect(json.find("\"sha256\": \"ba7816bf") != std::string::npos,
                     "JSON hash missing");

    return passed ? 0 : 1;
}

