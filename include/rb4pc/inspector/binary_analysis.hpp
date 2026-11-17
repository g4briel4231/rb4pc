#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace rb4pc::inspector {

constexpr std::size_t analysis_block_size = 64 * 1024;

struct BlockAnalysis {
    std::uint64_t offset = 0;
    std::size_t size = 0;
    double entropy = 0.0;
};

struct BinaryAnalysis {
    double entropy = 0.0;
    std::uintmax_t zero_bytes = 0;
    std::size_t block_size = analysis_block_size;
    std::size_t repeated_full_blocks = 0;
    std::vector<BlockAnalysis> blocks;
};

[[nodiscard]] double shannon_entropy(const unsigned char* data, std::size_t size);
[[nodiscard]] BinaryAnalysis analyze_binary(const std::filesystem::path& path);

} // namespace rb4pc::inspector

