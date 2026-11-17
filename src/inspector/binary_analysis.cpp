#include "rb4pc/inspector/binary_analysis.hpp"

#include <array>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace rb4pc::inspector {
namespace {

double entropy_from_counts(const std::array<std::uint64_t, 256>& counts, std::uint64_t size) {
    if (size == 0) return 0.0;
    double entropy = 0.0;
    for (const auto count : counts) {
        if (count == 0) continue;
        const auto probability = static_cast<double>(count) / static_cast<double>(size);
        entropy -= probability * std::log2(probability);
    }
    return entropy;
}

} // namespace

double shannon_entropy(const unsigned char* data, std::size_t size) {
    std::array<std::uint64_t, 256> counts{};
    for (std::size_t index = 0; index < size; ++index) ++counts[data[index]];
    return entropy_from_counts(counts, size);
}

BinaryAnalysis analyze_binary(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open file for analysis: " + path.string());

    BinaryAnalysis analysis;
    std::array<std::uint64_t, 256> total_counts{};
    std::vector<unsigned char> block(analysis.block_size);
    std::unordered_map<std::string, std::size_t> full_block_counts;
    std::uint64_t offset = 0;
    std::uint64_t total_size = 0;

    while (input) {
        input.read(reinterpret_cast<char*>(block.data()), static_cast<std::streamsize>(block.size()));
        const auto size = static_cast<std::size_t>(input.gcount());
        if (size == 0) break;

        for (std::size_t index = 0; index < size; ++index) {
            ++total_counts[block[index]];
            if (block[index] == 0) ++analysis.zero_bytes;
        }
        analysis.blocks.push_back({offset, size, shannon_entropy(block.data(), size)});
        if (size == analysis.block_size) {
            ++full_block_counts[std::string(reinterpret_cast<const char*>(block.data()), size)];
        }
        offset += size;
        total_size += size;
    }

    analysis.entropy = entropy_from_counts(total_counts, total_size);
    for (const auto& [content, count] : full_block_counts) {
        static_cast<void>(content);
        if (count > 1) analysis.repeated_full_blocks += count - 1;
    }
    return analysis;
}

} // namespace rb4pc::inspector
