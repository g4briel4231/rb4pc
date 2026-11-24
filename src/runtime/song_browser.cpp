#include "rb4pc/runtime/song_browser.hpp"

#include "rb4pc/runtime/artwork_texture.hpp"
#include "rb4pc/runtime/controller_icons.hpp"
#include "rb4pc/runtime/text.hpp"
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
    render_text(framebuffer, "RB4PC SONG LIBRARY", 24, 28, 0xfff4f6f8, 3);
    const auto selected = browser.empty() ? library.entries.size() : browser.selected_library_index();
    auto list_x = 80U;
    if (!browser.empty() && browser.selected().files.artwork && framebuffer.width() >= 360) {
        try {
            const auto artwork = decode_artwork_texture(*browser.selected().files.artwork);
            render_artwork(framebuffer, artwork, 24, 80, 128, 128);
            stroke_rect(framebuffer, {24, 80, 128, 128}, 0xff88a3bd, 2);
            list_x = 176;
        } catch (const std::exception&) {
        }
    }
    const auto available_width = framebuffer.width() > list_x ? framebuffer.width() - list_x : 0U;
    const auto width = available_width > 80 ? available_width - 80 : available_width;
    for (std::size_t index = 0; index < library.entries.size(); ++index) {
        const auto y = static_cast<std::uint32_t>(80 + index * 52);
        if (y + 36 >= framebuffer.height()) break;
        const auto color = !library.entries[index].valid ? 0xff6c2730
                         : index == selected ? 0xffd08a28 : 0xff263d52;
        fill_rect(framebuffer, {list_x, y, width, 36}, color);
        stroke_rect(framebuffer, {list_x, y, width, 36}, 0xff88a3bd, 1);
        const auto& entry = library.entries[index];
        render_text(framebuffer, entry.short_name, list_x + 12, y + 11, 0xfff4f6f8, 2);
        if (!entry.valid && width > 240)
            render_text(framebuffer, "INVALID", list_x + width - 100, y + 11, 0xffffc1c6, 2);
    }
    if (browser.empty()) {
        render_text(framebuffer, "NO VALID SONGS", 80, 120, 0xffffc1c6, 3);
        render_text(framebuffer, "ADD SONGS TO THE SONGS FOLDER", 80, 160, 0xffa9b8c7, 2);
    }
    if (framebuffer.width() >= 120 && framebuffer.height() >= 60) {
        render_controller_icon(framebuffer, ControllerPrompt::a,
                               {framebuffer.width() - 100, framebuffer.height() - 48, 32, 32});
        render_controller_icon(framebuffer, ControllerPrompt::b,
                               {framebuffer.width() - 52, framebuffer.height() - 48, 32, 32});
        if (framebuffer.width() >= 520) {
            render_text(framebuffer, "ENTER/A PLAY", framebuffer.width() - 420,
                        framebuffer.height() - 38, 0xfff4f6f8, 2);
            render_text(framebuffer, "ESC/B BACK", framebuffer.width() - 230,
                        framebuffer.height() - 38, 0xfff4f6f8, 2);
        }
    }
}

} // namespace rb4pc::runtime
