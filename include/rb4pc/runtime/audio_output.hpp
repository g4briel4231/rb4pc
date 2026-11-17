#pragma once

#include "rb4pc/runtime/wave_loader.hpp"

namespace rb4pc::runtime {
[[nodiscard]] bool audio_playback_available();
void play_wave_blocking(const ArchiveFileSystem& vfs, const WaveAsset& asset);
} // namespace rb4pc::runtime
