#include "rb4pc/inspector/archive_layout.hpp"
#include "rb4pc/inspector/murmur_hash3.hpp"

#include <limits>

namespace rb4pc::inspector {

namespace {
constexpr std::uint32_t path_hash_seed = 0x9520e3c4;
}

std::optional<ArchiveLocation> resolve_archive_location(
    const std::vector<HdrArchiveEntry>& archives,
    std::uint64_t global_offset,
    std::uint64_t size) {
    std::uint64_t archive_start = 0;
    for (std::size_t index = 0; index < archives.size(); ++index) {
        const auto archive_size = static_cast<std::uint64_t>(archives[index].declared_size);
        if (archive_start > std::numeric_limits<std::uint64_t>::max() - archive_size) return std::nullopt;
        const auto archive_end = archive_start + archive_size;
        const auto inside = global_offset >= archive_start && global_offset < archive_end;
        const auto zero_at_final_end = size == 0 && index + 1 == archives.size() && global_offset == archive_end;
        if (inside || zero_at_final_end) {
            const auto local_offset = global_offset - archive_start;
            if (size <= archive_size - local_offset) return ArchiveLocation{index, local_offset};
            return std::nullopt;
        }
        archive_start = archive_end;
    }
    return std::nullopt;
}

std::optional<std::size_t> find_file_record(const HdrMetadata& metadata, std::string_view path) {
    if (metadata.hash_bucket_heads.empty()) return std::nullopt;
    const auto hash = murmur_hash3_x64_128(path, path_hash_seed);
    auto index = metadata.hash_bucket_heads[hash.low % metadata.hash_bucket_heads.size()];
    while (index != std::numeric_limits<std::uint32_t>::max()) {
        if (index >= metadata.file_records.size()) return std::nullopt;
        const auto& record = metadata.file_records[index];
        if (record.path == path) return index;
        index = record.previous_hash_index;
    }
    return std::nullopt;
}

} // namespace rb4pc::inspector
