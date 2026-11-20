#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rb4pc::inspector {
struct TextureMipmap {
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t flags = 0;
    std::uint64_t data_offset = 0;
    std::uint32_t data_size = 0;
    std::string sha256;
};

struct TextureContainer {
    std::filesystem::path path;
    std::uint32_t magic = 0;
    std::uint32_t platform_version = 0;
    std::uint32_t marker = 0;
    std::uint32_t header_data_size = 0;
    std::uint32_t mipmap_entry_size = 0;
    std::uint32_t footer_size = 0;
    std::vector<TextureMipmap> mipmaps;
};

struct TextureCompatibility {
    bool compatible = false;
    std::string reason;
};

[[nodiscard]] TextureContainer inspect_texture_container(const std::filesystem::path& path);
[[nodiscard]] TextureCompatibility compare_texture_payloads(
    const TextureContainer& source, const TextureContainer& reference);
[[nodiscard]] std::string format_texture_report(const TextureContainer& texture);
} // namespace rb4pc::inspector
