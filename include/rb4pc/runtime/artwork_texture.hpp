#pragma once

#include "rb4pc/runtime/window.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace rb4pc::runtime {

struct DecodedArtwork {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint32_t> pixels;
};

[[nodiscard]] DecodedArtwork decode_artwork_texture(const std::filesystem::path& path);
void render_artwork(SoftwareFramebuffer& framebuffer, const DecodedArtwork& artwork,
                    std::uint32_t x, std::uint32_t y,
                    std::uint32_t width, std::uint32_t height);

} // namespace rb4pc::runtime
