#pragma once

#include "rb4pc/runtime/archive_file_system.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace rb4pc::runtime {
struct WaveAsset {
    std::string path;
    std::uint16_t format_tag = 0;
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint32_t byte_rate = 0;
    std::uint16_t block_align = 0;
    std::uint16_t bits_per_sample = 0;
    std::uint64_t data_offset = 0;
    std::uint64_t data_size = 0;
    std::uint64_t frame_count = 0;
    double duration_seconds = 0;
};
[[nodiscard]] WaveAsset load_wave(const ArchiveFileSystem& vfs, std::string_view path);
[[nodiscard]] std::vector<std::byte> read_pcm(
    const ArchiveFileSystem& vfs, const WaveAsset& asset,
    std::uint64_t data_offset, std::size_t size);
} // namespace rb4pc::runtime
