#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace rb4pc::runtime {

struct VorbisPcmPreview {
    std::uint32_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::size_t frames = 0;
    float peak_amplitude = 0.0F;
    bool finite_samples = false;
};

[[nodiscard]] VorbisPcmPreview decode_vorbis_preview(
    const std::filesystem::path& path, std::uintmax_t offset, std::size_t maximum_frames);

} // namespace rb4pc::runtime
