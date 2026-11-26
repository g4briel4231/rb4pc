#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>

namespace rb4pc::inspector {

struct ProtectedArkInfo {
    std::uint64_t stored_size = 0;
    std::uint64_t logical_size = 0;
    std::uint32_t key_material_size = 0;
    std::uint8_t initial_key = 0;
};

[[nodiscard]] std::optional<ProtectedArkInfo> inspect_protected_ark(
    const std::filesystem::path& path);

void copy_ark_range(const std::filesystem::path& path,
                    std::uint64_t logical_offset,
                    std::uint64_t size,
                    std::ostream& output);

} // namespace rb4pc::inspector