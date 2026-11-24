#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace rb4pc::builder {

struct PcAssetCatalog {
    std::size_t candidate_headers = 0;
    std::size_t valid_headers = 0;
    std::size_t invalid_headers = 0;
    std::size_t archive_volumes = 0;
    std::size_t records = 0;
    std::uint64_t payload_bytes = 0;
    std::size_t marker_records = 0;
    std::size_t native_path_records = 0;
    std::size_t intermediate_path_records = 0;
    std::size_t unknown_path_records = 0;
    std::map<std::string, std::size_t> namespaces;
    std::map<std::string, std::size_t> extensions;
    std::map<std::string, std::size_t> categories;
    std::map<std::string, std::size_t> unknown_extensions;
    std::vector<std::string> errors;
};

[[nodiscard]] PcAssetCatalog catalog_pc_assets(const std::filesystem::path& source);
[[nodiscard]] std::string format_json(const PcAssetCatalog& catalog);

} // namespace rb4pc::builder
