#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace rb4pc::inspector {
struct HeaderSetReport;
struct RiffConversionResult {
    std::filesystem::path destination;
    std::uint64_t record_size = 0;
    std::uint64_t logical_size = 0;
    std::string sha256;
    std::optional<std::filesystem::path> trailing_sidecar;
    std::optional<std::string> trailing_sha256;
};
[[nodiscard]] RiffConversionResult convert_riff_record(
    const HeaderSetReport& report, std::string_view internal_path,
    const std::filesystem::path& destination);
} // namespace rb4pc::inspector
