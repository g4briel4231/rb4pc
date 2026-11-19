#pragma once

#include "rb4pc/runtime/song_library.hpp"
#include "rb4pc/runtime/window.hpp"

#include <cstddef>
#include <vector>

namespace rb4pc::runtime {

class SongBrowserModel {
public:
    explicit SongBrowserModel(const SongLibrary& library);
    void move(int delta);
    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t selected_library_index() const;
    [[nodiscard]] const SongLibraryEntry& selected() const;
    [[nodiscard]] std::size_t valid_count() const;
private:
    const SongLibrary* library_ = nullptr;
    std::vector<std::size_t> valid_indices_;
    std::size_t selected_ = 0;
};

void render_song_browser(SoftwareFramebuffer& framebuffer, const SongLibrary& library,
                         const SongBrowserModel& browser, bool controller_connected);

} // namespace rb4pc::runtime
