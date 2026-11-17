#include "rb4pc/runtime/pcm_playback_queue.hpp"

#include <stdexcept>
#include <utility>

namespace rb4pc::runtime {

PcmPlaybackQueue::PcmPlaybackQueue(const ArchiveFileSystem& vfs, WaveAsset asset)
    : stream_(vfs, std::move(asset)),
      transport_(stream_.asset().sample_rate, stream_.asset().frame_count) {}
void PcmPlaybackQueue::play() { transport_.play(); }
void PcmPlaybackQueue::pause() { transport_.pause(); }
QueuedPcmChunk PcmPlaybackQueue::queue_next(std::size_t requested) {
    if (requested == 0) throw std::runtime_error("PCM queue request must contain frames");
    const auto first = stream_.frame_position();
    auto bytes = stream_.read_frames(requested);
    const auto frames = stream_.frame_position() - first;
    submitted_frames_ += frames;
    return {first, frames, std::move(bytes)};
}
void PcmPlaybackQueue::mark_completed(std::uint64_t frames) {
    if (frames > outstanding_frames()) throw std::runtime_error("PCM completion exceeds submitted frames");
    transport_.complete_frames(frames);
}
const AudioTransportClock& PcmPlaybackQueue::transport() const { return transport_; }
std::uint64_t PcmPlaybackQueue::submitted_frames() const { return submitted_frames_; }
std::uint64_t PcmPlaybackQueue::outstanding_frames() const {
    return submitted_frames_ - transport_.completed_frames();
}
bool PcmPlaybackQueue::source_exhausted() const { return stream_.remaining_frames() == 0; }

} // namespace rb4pc::runtime
