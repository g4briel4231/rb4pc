#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace rb4pc::runtime {

struct VorbisPcmPreview {
    std::uint32_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::size_t frames = 0;
    float peak_amplitude = 0.0F;
    bool finite_samples = false;
};

struct VorbisPcmBlock {
    std::uint64_t first_frame = 0;
    std::uint32_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::vector<float> samples;
    [[nodiscard]] std::size_t frame_count() const;
};

class VorbisPcmStream {
public:
    VorbisPcmStream(const std::filesystem::path& path, std::uintmax_t offset = 0);
    ~VorbisPcmStream();
    VorbisPcmStream(VorbisPcmStream&&) noexcept;
    VorbisPcmStream& operator=(VorbisPcmStream&&) noexcept;
    VorbisPcmStream(const VorbisPcmStream&) = delete;
    VorbisPcmStream& operator=(const VorbisPcmStream&) = delete;

    [[nodiscard]] std::uint32_t channels() const;
    [[nodiscard]] std::uint32_t sample_rate() const;
    [[nodiscard]] std::uint64_t total_frames() const;
    [[nodiscard]] std::uint64_t frame_position() const;
    [[nodiscard]] bool exhausted() const;
    void seek_frame(std::uint64_t frame);
    [[nodiscard]] VorbisPcmBlock read_frames(std::size_t maximum_frames);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] VorbisPcmPreview decode_vorbis_preview(
    const std::filesystem::path& path, std::uintmax_t offset, std::size_t maximum_frames);

} // namespace rb4pc::runtime
