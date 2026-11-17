#pragma once

#include <cstdint>

namespace rb4pc::runtime {

class AudioTransportClock {
public:
    AudioTransportClock(std::uint32_t sample_rate, std::uint64_t total_frames);
    void play();
    void pause();
    void seek_frame(std::uint64_t frame);
    void complete_frames(std::uint64_t frames);
    [[nodiscard]] std::uint32_t sample_rate() const;
    [[nodiscard]] std::uint64_t total_frames() const;
    [[nodiscard]] std::uint64_t completed_frames() const;
    [[nodiscard]] double seconds() const;
    [[nodiscard]] bool playing() const;
    [[nodiscard]] bool finished() const;
private:
    std::uint32_t sample_rate_ = 0;
    std::uint64_t total_frames_ = 0;
    std::uint64_t completed_frames_ = 0;
    bool playing_ = false;
};

} // namespace rb4pc::runtime
