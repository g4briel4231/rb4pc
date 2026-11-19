#pragma once

#include <cstdint>
#include <filesystem>

namespace rb4pc::runtime {

enum class RBMidFormat : std::uint32_t { rb4 = 0x10, rbvr = 0x2f };

struct RBMidHeader {
    RBMidFormat format = RBMidFormat::rb4;
    std::uintmax_t file_size = 0;
};

[[nodiscard]] RBMidHeader parse_rbmid_header(const std::filesystem::path& path);

} // namespace rb4pc::runtime
