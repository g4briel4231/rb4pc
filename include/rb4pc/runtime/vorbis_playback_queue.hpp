#pragma once

#include "rb4pc/runtime/audio_transport.hpp"
#include "rb4pc/runtime/vorbis_decoder.hpp"

namespace rb4pc::runtime {

class VorbisPlaybackQueue {
public:
    VorbisPlaybackQueue(const std::filesystem::path& path, std::uintmax_t offset = 0);
    void play();
    void pause();
    void seek_frame(std::uint64_t frame);
    [[nodiscard]] VorbisPcmBlock queue_next(std::size_t frame_count);
    void mark_completed(std::uint64_t frame_count);
    [[nodiscard]] const AudioTransportClock& transport() const;
    [[nodiscard]] std::uint64_t submitted_frames() const;
    [[nodiscard]] std::uint64_t outstanding_frames() const;
    [[nodiscard]] bool source_exhausted() const;
private:
    VorbisPcmStream stream_;
    AudioTransportClock transport_;
    std::uint64_t submitted_frames_ = 0;
};

} // namespace rb4pc::runtime
