#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rb4pc::inspector {

struct NativeCsv {
    std::uint32_t version = 0;
    std::uint32_t encoding = 0;
    std::uint8_t separator = 0;
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
};

[[nodiscard]] NativeCsv read_native_csv(const std::filesystem::path& path);
[[nodiscard]] std::string format_native_csv_summary_json(const NativeCsv& csv);

} // namespace rb4pc::inspector