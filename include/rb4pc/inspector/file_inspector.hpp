#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace rb4pc::inspector {

constexpr std::size_t preview_capacity = 32;

struct FileReport {
    std::filesystem::path path;
    std::uintmax_t size = 0;
    std::array<std::byte, preview_capacity> preview{};
    std::size_t preview_size = 0;
};

[[nodiscard]] FileReport inspect_file(const std::filesystem::path& path);
[[nodiscard]] std::string format_report(const FileReport& report);

} // namespace rb4pc::inspector

