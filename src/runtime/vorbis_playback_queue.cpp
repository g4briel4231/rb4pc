#include "rb4pc/runtime/vorbis_playback_queue.hpp"

#include <stdexcept>
#include <utility>

namespace rb4pc::runtime {

VorbisPlaybackQueue::VorbisPlaybackQueue(const std::filesystem::path& path, std::uintmax_t offset)
    : stream_(path, offset), transport_(stream_.sample_rate(), stream_.total_frames()) {}
void VorbisPlaybackQueue::play() { transport_.play(); }
void VorbisPlaybackQueue::pause() { transport_.pause(); }
void VorbisPlaybackQueue::seek_frame(std::uint64_t frame) {
    if (outstanding_frames() != 0) throw std::runtime_error("cannot seek with outstanding Vorbis frames");
    stream_.seek_frame(frame);
    transport_.seek_frame(frame);
    submitted_frames_ = frame;
}
VorbisPcmBlock VorbisPlaybackQueue::queue_next(std::size_t requested) {
    auto block = stream_.read_frames(requested);
    submitted_frames_ += block.frame_count();
    return block;
}
void VorbisPlaybackQueue::mark_completed(std::uint64_t frames) {
    if (frames > outstanding_frames()) throw std::runtime_error("Vorbis completion exceeds submitted frames");
    transport_.complete_frames(frames);
}
const AudioTransportClock& VorbisPlaybackQueue::transport() const { return transport_; }
std::uint64_t VorbisPlaybackQueue::submitted_frames() const { return submitted_frames_; }
std::uint64_t VorbisPlaybackQueue::outstanding_frames() const {
    return submitted_frames_ - transport_.completed_frames();
}
bool VorbisPlaybackQueue::source_exhausted() const { return stream_.exhausted(); }

} // namespace rb4pc::runtime
