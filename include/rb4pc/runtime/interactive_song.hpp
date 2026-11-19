#pragma once

#include "rb4pc/runtime/song_directory.hpp"

#include <cstddef>
#include <cstdint>

namespace rb4pc::runtime {

struct InteractiveSongResult {
    double seconds = 0.0;
    std::size_t hits = 0;
    std::size_t misses = 0;
    std::uint64_t score = 0;
    bool cancelled = false;
};

[[nodiscard]] InteractiveSongResult run_interactive_song(const SongDirectoryFiles& files);

} // namespace rb4pc::runtime
