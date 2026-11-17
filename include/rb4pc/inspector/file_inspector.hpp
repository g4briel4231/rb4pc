#pragma once

#include "rb4pc/inspector/binary_analysis.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rb4pc::inspector {

constexpr std::size_t preview_capacity = 32;

struct FileReport {
    std::filesystem::path path;
    std::uintmax_t size = 0;
    std::array<std::byte, preview_capacity> preview{};
    std::size_t preview_size = 0;
    std::string sha256;
};

struct ArchiveReport {
    std::filesystem::path path;
    std::uintmax_t size = 0;
    std::uint64_t index = 0;
};

struct HeaderSetReport {
    std::string platform_hint;
    FileReport header;
    BinaryAnalysis analysis;
    std::vector<ArchiveReport> archives;
};

[[nodiscard]] FileReport inspect_file(const std::filesystem::path& path);
[[nodiscard]] HeaderSetReport inspect_header_set(const std::filesystem::path& path);
[[nodiscard]] std::string format_report(const FileReport& report);
[[nodiscard]] std::string format_report(const HeaderSetReport& report);
[[nodiscard]] std::string format_json(const HeaderSetReport& report);

} // namespace rb4pc::inspector

