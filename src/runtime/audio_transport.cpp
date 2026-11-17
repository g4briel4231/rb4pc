#include "rb4pc/runtime/audio_transport.hpp"

#include <algorithm>
#include <stdexcept>

namespace rb4pc::runtime {

AudioTransportClock::AudioTransportClock(std::uint32_t sample_rate, std::uint64_t total_frames)
    : sample_rate_(sample_rate), total_frames_(total_frames) {
    if (sample_rate_ == 0) throw std::runtime_error("audio transport requires a sample rate");
}
void AudioTransportClock::play() { playing_ = completed_frames_ < total_frames_; }
void AudioTransportClock::pause() { playing_ = false; }
void AudioTransportClock::seek_frame(std::uint64_t frame) {
    if (frame > total_frames_) throw std::runtime_error("audio transport seek exceeds duration");
    completed_frames_ = frame; if (finished()) playing_ = false;
}
void AudioTransportClock::complete_frames(std::uint64_t frames) {
    if (!playing_) return;
    const auto remaining = total_frames_ - completed_frames_;
    completed_frames_ += std::min(frames, remaining);
    if (finished()) playing_ = false;
}
std::uint32_t AudioTransportClock::sample_rate() const { return sample_rate_; }
std::uint64_t AudioTransportClock::total_frames() const { return total_frames_; }
std::uint64_t AudioTransportClock::completed_frames() const { return completed_frames_; }
double AudioTransportClock::seconds() const { return static_cast<double>(completed_frames_) / sample_rate_; }
bool AudioTransportClock::playing() const { return playing_; }
bool AudioTransportClock::finished() const { return completed_frames_ == total_frames_; }

} // namespace rb4pc::runtime
