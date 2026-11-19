#include "rb4pc/runtime/song_browser.hpp"

#include "rb4pc/runtime/controller_icons.hpp"
#include "rb4pc/runtime/ui.hpp"

#include <algorithm>
#include <stdexcept>

namespace rb4pc::runtime {

SongBrowserModel::SongBrowserModel(const SongLibrary& library) : library_(&library) {
    for (std::size_t index = 0; index < library.entries.size(); ++index)
        if (library.entries[index].valid) valid_indices_.push_back(index);
}
void SongBrowserModel::move(int delta) {
    if (valid_indices_.empty()) return;
    const auto count = static_cast<long long>(valid_indices_.size());
    auto next = (static_cast<long long>(selected_) + delta) % count;
    if (next < 0) next += count;
    selected_ = static_cast<std::size_t>(next);
}
bool SongBrowserModel::empty() const { return valid_indices_.empty(); }
std::size_t SongBrowserModel::selected_library_index() const {
    if (empty()) throw std::runtime_error("song browser has no valid selection");
    return valid_indices_[selected_];
}
const SongLibraryEntry& SongBrowserModel::selected() const {
    return library_->entries[selected_library_index()];
}
std::size_t SongBrowserModel::valid_count() const { return valid_indices_.size(); }

void render_song_browser(SoftwareFramebuffer& framebuffer, const SongLibrary& library,
                         const SongBrowserModel& browser, bool controller_connected) {
    framebuffer.clear(0xff0e1219);
    fill_rect(framebuffer, {0, 0, framebuffer.width(), 10},
              controller_connected ? 0xff2a9d50 : 0xffb43b3b);
    const auto selected = browser.empty() ? library.entries.size() : browser.selected_library_index();
    const auto width = framebuffer.width() > 160 ? framebuffer.width() - 160 : framebuffer.width();
    for (std::size_t index = 0; index < library.entries.size(); ++index) {
        const auto y = static_cast<std::uint32_t>(80 + index * 52);
        if (y + 36 >= framebuffer.height()) break;
        const auto color = !library.entries[index].valid ? 0xff6c2730
                         : index == selected ? 0xffd08a28 : 0xff263d52;
        fill_rect(framebuffer, {80, y, width, 36}, color);
        stroke_rect(framebuffer, {80, y, width, 36}, 0xff88a3bd, 1);
    }
    if (framebuffer.width() >= 120 && framebuffer.height() >= 60) {
        render_controller_icon(framebuffer, ControllerPrompt::a,
                               {framebuffer.width() - 100, framebuffer.height() - 48, 32, 32});
        render_controller_icon(framebuffer, ControllerPrompt::b,
                               {framebuffer.width() - 52, framebuffer.height() - 48, 32, 32});
    }
}

} // namespace rb4pc::runtime
