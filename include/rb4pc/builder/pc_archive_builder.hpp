#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rb4pc::builder {

struct PcArchiveBuildResult {
    std::filesystem::path header;
    std::vector<std::filesystem::path> archives;
    std::size_t record_count = 0;
    std::uint64_t payload_bytes = 0;
    std::string header_sha256;
    std::vector<std::string> archive_sha256;
};

struct PcArchiveBuildOptions {
    std::uint32_t maximum_archive_bytes = 0xffffffffU;
};

[[nodiscard]] PcArchiveBuildResult build_pc_song_archive(
    const std::filesystem::path& song_directory,
    const std::filesystem::path& output_directory,
    std::string_view short_name,
    PcArchiveBuildOptions options = {});

} // namespace rb4pc::builder
