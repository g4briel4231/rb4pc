#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace rb4pc::builder {

struct PcMetadataWordStats {
    std::size_t observed = 0;
    std::size_t zero_values = 0;
    std::size_t values_equal_record_size = 0;
    std::size_t values_equal_remaining_size = 0;
    std::uint32_t minimum = 0;
    std::uint32_t maximum = 0;
    std::set<std::uint32_t> distinct_values;
};

struct PcMetadataFamilyStats {
    std::size_t records = 0;
    std::uint64_t bytes = 0;
    std::uint32_t minimum_size = 0;
    std::uint32_t maximum_size = 0;
    std::size_t empty_records = 0;
    std::size_t unreadable_records = 0;
    std::size_t records_with_ascii = 0;
    std::size_t ascii_runs = 0;
    std::size_t null_terminated_ascii_runs = 0;
    std::size_t maximum_ascii_length = 0;
    std::map<std::string, std::size_t> first_sixteen_bytes;
    std::map<std::string, std::size_t> first_u32_le;
    std::map<std::size_t, std::size_t> ascii_length_distribution;
    std::map<std::size_t, PcMetadataWordStats> prefix_u32_fields;
};

struct PcMetadataCatalog {
    std::size_t candidate_headers = 0;
    std::size_t valid_headers = 0;
    std::size_t invalid_headers = 0;
    std::size_t records = 0;
    std::uint64_t bytes = 0;
    std::size_t unreadable_records = 0;
    std::map<std::string, PcMetadataFamilyStats> families;
    std::vector<std::string> errors;
};

[[nodiscard]] PcMetadataCatalog catalog_pc_metadata(const std::filesystem::path& source);
[[nodiscard]] std::string format_json(const PcMetadataCatalog& catalog);

} // namespace rb4pc::builder