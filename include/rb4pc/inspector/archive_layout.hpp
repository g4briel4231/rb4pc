#pragma once

#include "rb4pc/inspector/hdr_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace rb4pc::inspector {

struct ArchiveLocation {
    std::size_t archive_index = 0;
    std::uint64_t local_offset = 0;
};

[[nodiscard]] std::optional<ArchiveLocation> resolve_archive_location(
    const std::vector<HdrArchiveEntry>& archives,
    std::uint64_t global_offset,
    std::uint64_t size);
[[nodiscard]] std::optional<std::size_t> find_file_record(
    const HdrMetadata& metadata,
    std::string_view path);

} // namespace rb4pc::inspector
