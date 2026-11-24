#include "rb4pc/runtime/text.hpp"

#include "rb4pc/runtime/ui.hpp"

#include <array>
#include <cctype>

namespace rb4pc::runtime {
namespace {
using Glyph = std::array<std::uint8_t, 7>;

Glyph glyph(char value) {
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(value)))) {
    case 'A': return {14, 17, 17, 31, 17, 17, 17};
    case 'B': return {30, 17, 17, 30, 17, 17, 30};
    case 'C': return {14, 17, 16, 16, 16, 17, 14};
    case 'D': return {30, 17, 17, 17, 17, 17, 30};
    case 'E': return {31, 16, 16, 30, 16, 16, 31};
    case 'F': return {31, 16, 16, 30, 16, 16, 16};
    case 'G': return {14, 17, 16, 23, 17, 17, 15};
    case 'H': return {17, 17, 17, 31, 17, 17, 17};
    case 'I': return {31, 4, 4, 4, 4, 4, 31};
    case 'J': return {7, 2, 2, 2, 18, 18, 12};
    case 'K': return {17, 18, 20, 24, 20, 18, 17};
    case 'L': return {16, 16, 16, 16, 16, 16, 31};
    case 'M': return {17, 27, 21, 21, 17, 17, 17};
    case 'N': return {17, 25, 21, 19, 17, 17, 17};
    case 'O': return {14, 17, 17, 17, 17, 17, 14};
    case 'P': return {30, 17, 17, 30, 16, 16, 16};
    case 'Q': return {14, 17, 17, 17, 21, 18, 13};
    case 'R': return {30, 17, 17, 30, 20, 18, 17};
    case 'S': return {15, 16, 16, 14, 1, 1, 30};
    case 'T': return {31, 4, 4, 4, 4, 4, 4};
    case 'U': return {17, 17, 17, 17, 17, 17, 14};
    case 'V': return {17, 17, 17, 17, 17, 10, 4};
    case 'W': return {17, 17, 17, 21, 21, 21, 10};
    case 'X': return {17, 17, 10, 4, 10, 17, 17};
    case 'Y': return {17, 17, 10, 4, 4, 4, 4};
    case 'Z': return {31, 1, 2, 4, 8, 16, 31};
    case '0': return {14, 17, 19, 21, 25, 17, 14};
    case '1': return {4, 12, 4, 4, 4, 4, 14};
    case '2': return {14, 17, 1, 2, 4, 8, 31};
    case '3': return {30, 1, 1, 14, 1, 1, 30};
    case '4': return {2, 6, 10, 18, 31, 2, 2};
    case '5': return {31, 16, 16, 30, 1, 1, 30};
    case '6': return {14, 16, 16, 30, 17, 17, 14};
    case '7': return {31, 1, 2, 4, 8, 8, 8};
    case '8': return {14, 17, 17, 14, 17, 17, 14};
    case '9': return {14, 17, 17, 15, 1, 1, 14};
    case '-': return {0, 0, 0, 31, 0, 0, 0};
    case '_': return {0, 0, 0, 0, 0, 0, 31};
    case '.': return {0, 0, 0, 0, 0, 12, 12};
    case ':': return {0, 12, 12, 0, 12, 12, 0};
    case '/': return {1, 2, 2, 4, 8, 8, 16};
    case '!': return {4, 4, 4, 4, 4, 0, 4};
    case '?': return {14, 17, 1, 2, 4, 0, 4};
    default: return {};
    }
}
} // namespace

TextExtent measure_text(std::string_view text, std::uint32_t scale) {
    if (text.empty() || scale == 0) return {};
    return {static_cast<std::uint32_t>((text.size() * 6 - 1) * scale), 7 * scale};
}

void render_text(SoftwareFramebuffer& framebuffer, std::string_view text,
                 std::uint32_t x, std::uint32_t y, std::uint32_t color,
                 std::uint32_t scale) {
    if (scale == 0) return;
    for (std::size_t character = 0; character < text.size(); ++character) {
        const auto rows = glyph(text[character]);
        for (std::uint32_t row = 0; row < rows.size(); ++row) {
            for (std::uint32_t column = 0; column < 5; ++column) {
                if ((rows[row] & (1U << (4U - column))) != 0)
                    fill_rect(framebuffer,
                              {x + static_cast<std::uint32_t>(character * 6) * scale + column * scale,
                               y + row * scale, scale, scale}, color);
            }
        }
    }
}

} // namespace rb4pc::runtime
