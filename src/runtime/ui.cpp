#include "rb4pc/runtime/ui.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rb4pc::runtime {
void fill_rect(SoftwareFramebuffer& fb, Rect r, std::uint32_t color) {
    const auto right = std::min<std::uint64_t>(fb.width(), static_cast<std::uint64_t>(r.x) + r.width);
    const auto bottom = std::min<std::uint64_t>(fb.height(), static_cast<std::uint64_t>(r.y) + r.height);
    for (std::uint32_t y = r.y; y < bottom; ++y)
        for (std::uint32_t x = r.x; x < right; ++x) fb.set_pixel(x, y, color);
}
void stroke_rect(SoftwareFramebuffer& fb, Rect r, std::uint32_t color, std::uint32_t t) {
    if (t == 0) return;
    fill_rect(fb, {r.x, r.y, r.width, std::min(t, r.height)}, color);
    fill_rect(fb, {r.x, r.y + (r.height > t ? r.height - t : 0), r.width, std::min(t, r.height)}, color);
    fill_rect(fb, {r.x, r.y, std::min(t, r.width), r.height}, color);
    fill_rect(fb, {r.x + (r.width > t ? r.width - t : 0), r.y, std::min(t, r.width), r.height}, color);
}
MenuModel::MenuModel(std::vector<std::string> items) : items_(std::move(items)) {
    if (items_.empty()) throw std::runtime_error("menu requires at least one item");
}
void MenuModel::move(int delta) {
    const auto count = static_cast<long long>(items_.size());
    auto next = (static_cast<long long>(selected_) + delta) % count; if (next < 0) next += count;
    selected_ = static_cast<std::size_t>(next);
}
std::size_t MenuModel::selected_index() const { return selected_; }
const std::string& MenuModel::selected_item() const { return items_[selected_]; }
const std::vector<std::string>& MenuModel::items() const { return items_; }
} // namespace rb4pc::runtime
