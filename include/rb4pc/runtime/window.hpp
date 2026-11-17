#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace rb4pc::runtime {
class SoftwareFramebuffer {
public:
    SoftwareFramebuffer(std::uint32_t width, std::uint32_t height);
    [[nodiscard]] std::uint32_t width() const;
    [[nodiscard]] std::uint32_t height() const;
    void clear(std::uint32_t bgra);
    void set_pixel(std::uint32_t x, std::uint32_t y, std::uint32_t bgra);
    [[nodiscard]] const std::vector<std::uint32_t>& pixels() const;
private:
    std::uint32_t width_, height_;
    std::vector<std::uint32_t> pixels_;
};
[[nodiscard]] bool window_backend_available();
void run_window(std::string_view title, SoftwareFramebuffer& framebuffer);
} // namespace rb4pc::runtime
