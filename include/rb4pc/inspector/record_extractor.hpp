#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace rb4pc::inspector {

struct HeaderSetReport;

struct ExtractionResult {
    std::filesystem::path destination;
    std::uint64_t size = 0;
    std::string sha256;
};

[[nodiscard]] ExtractionResult extract_record(
    const HeaderSetReport& report,
    std::string_view internal_path,
    const std::filesystem::path& destination);

} // namespace rb4pc::inspector
