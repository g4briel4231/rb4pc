#pragma once

#include "rb4pc/runtime/window.hpp"

#include <cstdint>
#include <string_view>

namespace rb4pc::runtime {

struct TextExtent {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

[[nodiscard]] TextExtent measure_text(std::string_view text, std::uint32_t scale = 1);
void render_text(SoftwareFramebuffer& framebuffer, std::string_view text,
                 std::uint32_t x, std::uint32_t y, std::uint32_t color,
                 std::uint32_t scale = 1);

} // namespace rb4pc::runtime
