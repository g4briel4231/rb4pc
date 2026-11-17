#pragma once

#include "rb4pc/runtime/wave_loader.hpp"

namespace rb4pc::runtime {
class PcmStream {
public:
    PcmStream(const ArchiveFileSystem& vfs, WaveAsset asset);
    [[nodiscard]] const WaveAsset& asset() const;
    [[nodiscard]] std::uint64_t frame_position() const;
    [[nodiscard]] std::uint64_t remaining_frames() const;
    void seek_frame(std::uint64_t frame);
    [[nodiscard]] std::vector<std::byte> read_frames(std::size_t frame_count);
private:
    const ArchiveFileSystem* vfs_;
    WaveAsset asset_;
    std::uint64_t frame_position_ = 0;
};
} // namespace rb4pc::runtime
