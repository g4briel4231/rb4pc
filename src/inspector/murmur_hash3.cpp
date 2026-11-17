#include "rb4pc/inspector/murmur_hash3.hpp"

#include <bit>

namespace rb4pc::inspector {
namespace {

constexpr std::uint64_t c1 = 0x87c37b91114253d5ULL;
constexpr std::uint64_t c2 = 0x4cf5ad432745937fULL;

[[nodiscard]] std::uint64_t load_u64_le(const unsigned char* data) {
    std::uint64_t value = 0;
    for (unsigned int index = 0; index < 8; ++index) {
        value |= static_cast<std::uint64_t>(data[index]) << (index * 8);
    }
    return value;
}

[[nodiscard]] std::uint64_t fmix64(std::uint64_t value) {
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33;
    return value;
}

} // namespace

MurmurHash128 murmur_hash3_x64_128(const unsigned char* data, std::size_t size, std::uint32_t seed) {
    std::uint64_t h1 = seed;
    std::uint64_t h2 = seed;
    const auto block_count = size / 16;

    for (std::size_t index = 0; index < block_count; ++index) {
        const auto* block = data + index * 16;
        auto k1 = load_u64_le(block);
        auto k2 = load_u64_le(block + 8);

        k1 *= c1;
        k1 = std::rotl(k1, 31);
        k1 *= c2;
        h1 ^= k1;
        h1 = std::rotl(h1, 27);
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729;

        k2 *= c2;
        k2 = std::rotl(k2, 33);
        k2 *= c1;
        h2 ^= k2;
        h2 = std::rotl(h2, 31);
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5;
    }

    const auto* tail = data + block_count * 16;
    const auto tail_size = size & 15;
    std::uint64_t k1 = 0;
    std::uint64_t k2 = 0;
    for (std::size_t index = 8; index < tail_size; ++index) {
        k2 |= static_cast<std::uint64_t>(tail[index]) << ((index - 8) * 8);
    }
    if (tail_size > 8) {
        k2 *= c2;
        k2 = std::rotl(k2, 33);
        k2 *= c1;
        h2 ^= k2;
    }
    const auto first_tail_size = tail_size < 8 ? tail_size : 8;
    for (std::size_t index = 0; index < first_tail_size; ++index) {
        k1 |= static_cast<std::uint64_t>(tail[index]) << (index * 8);
    }
    if (first_tail_size != 0) {
        k1 *= c1;
        k1 = std::rotl(k1, 31);
        k1 *= c2;
        h1 ^= k1;
    }

    h1 ^= size;
    h2 ^= size;
    h1 += h2;
    h2 += h1;
    h1 = fmix64(h1);
    h2 = fmix64(h2);
    h1 += h2;
    h2 += h1;
    return {h1, h2};
}

MurmurHash128 murmur_hash3_x64_128(std::string_view value, std::uint32_t seed) {
    return murmur_hash3_x64_128(reinterpret_cast<const unsigned char*>(value.data()), value.size(), seed);
}

} // namespace rb4pc::inspector
