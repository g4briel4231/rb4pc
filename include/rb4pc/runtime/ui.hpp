#pragma once
#include "rb4pc/runtime/window.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rb4pc::runtime {
struct Rect { std::uint32_t x, y, width, height; };
void fill_rect(SoftwareFramebuffer& framebuffer, Rect rect, std::uint32_t color);
void stroke_rect(SoftwareFramebuffer& framebuffer, Rect rect, std::uint32_t color, std::uint32_t thickness);
class MenuModel {
public:
    explicit MenuModel(std::vector<std::string> items);
    void move(int delta);
    [[nodiscard]] std::size_t selected_index() const;
    [[nodiscard]] const std::string& selected_item() const;
    [[nodiscard]] const std::vector<std::string>& items() const;
private:
    std::vector<std::string> items_;
    std::size_t selected_ = 0;
};
} // namespace rb4pc::runtime
