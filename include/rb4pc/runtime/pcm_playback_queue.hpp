#pragma once

#include "rb4pc/runtime/audio_transport.hpp"
#include "rb4pc/runtime/pcm_stream.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rb4pc::runtime {

struct QueuedPcmChunk {
    std::uint64_t first_frame = 0;
    std::uint64_t frame_count = 0;
    std::vector<std::byte> bytes;
};

class PcmPlaybackQueue {
public:
    PcmPlaybackQueue(const ArchiveFileSystem& vfs, WaveAsset asset);
    void play();
    void pause();
    [[nodiscard]] QueuedPcmChunk queue_next(std::size_t frame_count);
    void mark_completed(std::uint64_t frame_count);
    [[nodiscard]] const AudioTransportClock& transport() const;
    [[nodiscard]] std::uint64_t submitted_frames() const;
    [[nodiscard]] std::uint64_t outstanding_frames() const;
    [[nodiscard]] bool source_exhausted() const;
private:
    PcmStream stream_;
    AudioTransportClock transport_;
    std::uint64_t submitted_frames_ = 0;
};

} // namespace rb4pc::runtime
