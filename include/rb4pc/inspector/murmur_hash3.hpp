#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace rb4pc::inspector {

struct MurmurHash128 {
    std::uint64_t low = 0;
    std::uint64_t high = 0;
};

[[nodiscard]] MurmurHash128 murmur_hash3_x64_128(const unsigned char* data,
                                                  std::size_t size,
                                                  std::uint32_t seed);
[[nodiscard]] MurmurHash128 murmur_hash3_x64_128(std::string_view value, std::uint32_t seed);

} // namespace rb4pc::inspector
