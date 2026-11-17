#pragma once

#include "rb4pc/inspector/binary_analysis.hpp"
#include "rb4pc/inspector/archive_layout.hpp"
#include "rb4pc/inspector/hdr_decoder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
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
    std::uint32_t declared_size = 0;
    std::uintmax_t actual_size = 0;
    bool exists = false;
    bool size_matches = false;
};

struct HeaderSetReport {
    std::string platform_hint;
    FileReport header;
    BinaryAnalysis analysis;
    HdrMetadata metadata;
    std::vector<ArchiveReport> archives;
    std::vector<std::optional<ArchiveLocation>> record_locations;
};

struct RecordPreview {
    std::size_t record_index = 0;
    ArchiveLocation location;
    std::array<std::byte, preview_capacity> bytes{};
    std::size_t size = 0;
};

[[nodiscard]] FileReport inspect_file(const std::filesystem::path& path);
[[nodiscard]] HeaderSetReport inspect_header_set(const std::filesystem::path& path);
[[nodiscard]] std::optional<RecordPreview> inspect_record(
    const HeaderSetReport& report,
    std::string_view internal_path);
[[nodiscard]] std::string format_report(const FileReport& report);
[[nodiscard]] std::string format_report(const HeaderSetReport& report);
[[nodiscard]] std::string format_json(const HeaderSetReport& report);
[[nodiscard]] std::string format_report(const HeaderSetReport& report, const RecordPreview& preview);

} // namespace rb4pc::inspector

