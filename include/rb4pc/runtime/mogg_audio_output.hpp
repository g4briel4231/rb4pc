#pragma once

#include "rb4pc/runtime/audio_output.hpp"
#include "rb4pc/runtime/mogg_mix.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace rb4pc::runtime {

[[nodiscard]] std::vector<std::int16_t> stereo_float_to_pcm16(const StereoPcmBlock& block);
void play_mogg_blocking(const std::filesystem::path& mogg_path,
                        const std::filesystem::path& mix_metadata_path,
                        const MoggMixConfig& config = {},
                        const AudioProgressCallback& progress = {});

} // namespace rb4pc::runtime
