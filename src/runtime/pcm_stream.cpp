#include "rb4pc/runtime/pcm_stream.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rb4pc::runtime {
PcmStream::PcmStream(const ArchiveFileSystem& vfs, WaveAsset asset)
    : vfs_(&vfs), asset_(std::move(asset)) {
    if (asset_.block_align == 0) throw std::runtime_error("PCM stream has zero block alignment");
}
const WaveAsset& PcmStream::asset() const { return asset_; }
std::uint64_t PcmStream::frame_position() const { return frame_position_; }
std::uint64_t PcmStream::remaining_frames() const { return asset_.frame_count - frame_position_; }
void PcmStream::seek_frame(std::uint64_t frame) {
    if (frame > asset_.frame_count) throw std::runtime_error("PCM seek exceeds frame count");
    frame_position_ = frame;
}
std::vector<std::byte> PcmStream::read_frames(std::size_t requested) {
    const auto frames = std::min<std::uint64_t>(requested, remaining_frames());
    if (frames > std::numeric_limits<std::size_t>::max() / asset_.block_align) {
        throw std::runtime_error("PCM frame read size overflow");
    }
    const auto bytes = static_cast<std::size_t>(frames * asset_.block_align);
    auto result = read_pcm(*vfs_, asset_, frame_position_ * asset_.block_align, bytes);
    frame_position_ += frames;
    return result;
}
} // namespace rb4pc::runtime
