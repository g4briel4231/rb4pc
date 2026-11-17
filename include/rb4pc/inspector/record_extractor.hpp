#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <optional>

namespace rb4pc::inspector {

struct HeaderSetReport;

struct ExtractionResult {
    std::filesystem::path destination;
    std::uint64_t size = 0;
    std::string sha256;
};

struct SelectiveExtractionOptions {
    std::optional<std::string> prefix;
    std::optional<std::string> extension;
};

struct SelectiveExtractionResult {
    std::filesystem::path destination_directory;
    std::filesystem::path manifest;
    std::size_t file_count = 0;
    std::uint64_t total_size = 0;
};

[[nodiscard]] ExtractionResult extract_record(
    const HeaderSetReport& report,
    std::string_view internal_path,
    const std::filesystem::path& destination);
[[nodiscard]] SelectiveExtractionResult extract_records(
    const HeaderSetReport& report,
    const SelectiveExtractionOptions& options,
    const std::filesystem::path& destination_directory);

} // namespace rb4pc::inspector
