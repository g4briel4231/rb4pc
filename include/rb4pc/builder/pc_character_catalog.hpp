#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace rb4pc::builder {

struct PcCharacterFamilyStats {
    std::size_t records = 0;
    std::uint64_t bytes = 0;
    std::uint32_t minimum_size = 0;
    std::uint32_t maximum_size = 0;
    std::map<std::string, std::size_t> first_four_bytes;
    std::map<std::string, std::size_t> first_u32_le;
};

struct PcCharacterCatalog {
    std::size_t candidate_headers = 0;
    std::size_t valid_headers = 0;
    std::size_t invalid_headers = 0;
    std::size_t records = 0;
    std::uint64_t bytes = 0;
    std::size_t empty_records = 0;
    std::size_t unreadable_records = 0;
    std::map<std::string, PcCharacterFamilyStats> families;
    std::vector<std::string> errors;
};

[[nodiscard]] PcCharacterCatalog catalog_pc_characters(const std::filesystem::path& source);
[[nodiscard]] std::string format_json(const PcCharacterCatalog& catalog);

} // namespace rb4pc::builder
