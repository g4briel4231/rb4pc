#pragma once

#include "rb4pc/runtime/wave_loader.hpp"

#include <cstdint>
#include <functional>

namespace rb4pc::runtime {
using AudioProgressCallback = std::function<void(std::uint64_t completed_frames)>;
[[nodiscard]] bool audio_playback_available();
void play_wave_blocking(const ArchiveFileSystem& vfs, const WaveAsset& asset);
void play_wave_blocking(const ArchiveFileSystem& vfs, const WaveAsset& asset, const AudioProgressCallback& progress);
} // namespace rb4pc::runtime
