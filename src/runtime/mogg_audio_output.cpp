#include "rb4pc/runtime/mogg_audio_output.hpp"

#include "rb4pc/runtime/mogg_parser.hpp"
#include "rb4pc/runtime/vorbis_playback_queue.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#endif

namespace rb4pc::runtime {

std::vector<std::int16_t> stereo_float_to_pcm16(const StereoPcmBlock& block) {
    if (block.samples.size() % 2 != 0) throw std::runtime_error("stereo PCM block has an odd sample count");
    std::vector<std::int16_t> result;
    result.reserve(block.samples.size());
    for (const auto sample : block.samples) {
        if (!std::isfinite(sample)) throw std::runtime_error("stereo PCM block contains a non-finite sample");
        const auto limited = std::clamp(sample, -1.0F, 1.0F);
        result.push_back(static_cast<std::int16_t>(std::lround(limited * 32767.0F)));
    }
    return result;
}

void play_mogg_blocking(const std::filesystem::path& mogg_path,
                        const std::filesystem::path& mix_metadata_path,
                        const MoggMixConfig& config,
                        const AudioProgressCallback& progress,
                        const AudioCancellationCallback& cancellation) {
#ifdef _WIN32
    const auto header = parse_mogg_header(mogg_path);
    const auto vorbis = inspect_ogg_vorbis(mogg_path, header.ogg_offset);
    const auto metadata = parse_mogg_mix_metadata(mix_metadata_path, vorbis.channels);
    VorbisPlaybackQueue queue(mogg_path, header.ogg_offset);

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = queue.transport().sample_rate();
    format.wBitsPerSample = 16;
    format.nBlockAlign = 4;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    HWAVEOUT output = nullptr;
    if (waveOutOpen(&output, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        throw std::runtime_error("waveOutOpen failed for MOGG stereo output");

    struct OutputBuffer {
        std::vector<std::int16_t> samples;
        WAVEHDR header{};
        std::uint64_t frames = 0;
        bool prepared = false;
        bool active = false;
    };
    std::array<OutputBuffer, 2> buffers;
    queue.play();
    auto submit = [&](OutputBuffer& buffer) {
        if (queue.source_exhausted()) return false;
        const auto decoded = queue.queue_next(1024);
        if (decoded.frame_count() == 0) throw std::runtime_error("MOGG audio decoder stalled");
        const auto mixed = mix_mogg_to_stereo(decoded, metadata, config);
        buffer.samples = stereo_float_to_pcm16(mixed);
        buffer.frames = mixed.frame_count();
        buffer.header = {};
        buffer.header.lpData = reinterpret_cast<LPSTR>(buffer.samples.data());
        buffer.header.dwBufferLength = static_cast<DWORD>(buffer.samples.size() * sizeof(std::int16_t));
        if (waveOutPrepareHeader(output, &buffer.header, sizeof(buffer.header)) != MMSYSERR_NOERROR)
            throw std::runtime_error("waveOutPrepareHeader failed for MOGG buffer");
        buffer.prepared = true;
        if (waveOutWrite(output, &buffer.header, sizeof(buffer.header)) != MMSYSERR_NOERROR)
            throw std::runtime_error("waveOutWrite failed for MOGG buffer");
        buffer.active = true;
        return true;
    };

    bool cancelled = false;
    try {
        for (auto& buffer : buffers) static_cast<void>(submit(buffer));
        bool any_active = true;
        while (any_active) {
            if (cancellation && cancellation()) { cancelled = true; waveOutReset(output); break; }
            any_active = false;
            for (auto& buffer : buffers) {
                if (!buffer.active) continue;
                any_active = true;
                if ((buffer.header.dwFlags & WHDR_DONE) == 0) continue;
                queue.mark_completed(buffer.frames);
                if (progress) progress(queue.transport().completed_frames());
                waveOutUnprepareHeader(output, &buffer.header, sizeof(buffer.header));
                buffer.prepared = false;
                buffer.active = false;
                if (submit(buffer)) any_active = true;
            }
            if (any_active) Sleep(2);
        }
    } catch (...) {
        waveOutReset(output);
        for (auto& buffer : buffers)
            if (buffer.prepared) waveOutUnprepareHeader(output, &buffer.header, sizeof(buffer.header));
        waveOutClose(output);
        throw;
    }
    for (auto& buffer : buffers)
        if (buffer.prepared) waveOutUnprepareHeader(output, &buffer.header, sizeof(buffer.header));
    waveOutClose(output);
    if (!cancelled && !queue.transport().finished())
        throw std::runtime_error("MOGG playback ended before transport completion");
#else
    (void)mogg_path; (void)mix_metadata_path; (void)config; (void)progress; (void)cancellation;
    throw std::runtime_error("MOGG audio playback backend is unavailable on this platform");
#endif
}

} // namespace rb4pc::runtime
