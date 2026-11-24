#include "rb4pc/runtime/window.hpp"
#include <iostream>

int main() {
    try {
        rb4pc::runtime::SoftwareFramebuffer framebuffer(960, 540);
        for (std::uint32_t y = 0; y < framebuffer.height(); ++y)
            for (std::uint32_t x = 0; x < framebuffer.width(); ++x)
                framebuffer.set_pixel(x, y, 0xff000000U | ((x * 255 / framebuffer.width()) << 16) |
                    ((y * 255 / framebuffer.height()) << 8) | 0x30U);
        rb4pc::runtime::run_window("RB4PC v0.150.0 Runtime Window", framebuffer);
    } catch (const std::exception& error) { std::cerr << "rb4pc-window: " << error.what() << '\n'; return 1; }
}
