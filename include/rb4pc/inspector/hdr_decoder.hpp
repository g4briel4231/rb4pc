#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rb4pc::inspector {

struct HdrArchiveEntry {
    std::string name;
    std::uint32_t declared_size = 0;
};

struct HdrMetadata {
    std::uint32_t raw_seed = 0;
    std::uint32_t version = 0;
    std::uint32_t unknown_04 = 0;
    std::array<unsigned char, 16> unknown_08{};
    std::vector<HdrArchiveEntry> archives;
};

[[nodiscard]] HdrMetadata decode_hdr_prefix(const std::filesystem::path& path);
[[nodiscard]] std::string hex_string(const unsigned char* data, std::size_t size);

} // namespace rb4pc::inspector

