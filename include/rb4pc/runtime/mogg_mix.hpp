#pragma once

#include "rb4pc/runtime/vorbis_decoder.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace rb4pc::runtime {

struct MoggStemChannels {
    std::string name;
    std::vector<std::uint32_t> channels;
};

struct MoggMixMetadata {
    std::vector<MoggStemChannels> stems;
    std::vector<float> pans;
    std::vector<float> volumes_db;
};

struct MoggMixConfig {
    std::unordered_map<std::string, float> stem_levels;
    float master_level = 1.0F;
    bool clamp_output = true;
};

struct StereoPcmBlock {
    std::uint64_t first_frame = 0;
    std::uint32_t sample_rate = 0;
    std::vector<float> samples;
    float peak_amplitude = 0.0F;
    float pre_clamp_peak_amplitude = 0.0F;
    std::uint64_t clipped_samples = 0;
    [[nodiscard]] std::size_t frame_count() const;
};

[[nodiscard]] MoggMixMetadata parse_mogg_mix_metadata(
    const std::filesystem::path& path, std::uint32_t expected_channels);
[[nodiscard]] StereoPcmBlock mix_mogg_to_stereo(
    const VorbisPcmBlock& source, const MoggMixMetadata& metadata,
    const MoggMixConfig& config = {});

} // namespace rb4pc::runtime
