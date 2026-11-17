#include "rb4pc/runtime/audio_output.hpp"

#include <algorithm>
#include <exception>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#endif

namespace rb4pc::runtime {
bool audio_playback_available() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

void play_wave_blocking(const ArchiveFileSystem& vfs, const WaveAsset& asset) {
    play_wave_blocking(vfs, asset, {});
}

void play_wave_blocking(const ArchiveFileSystem& vfs, const WaveAsset& asset, const AudioProgressCallback& progress) {
#ifdef _WIN32
    if (asset.data_size > std::numeric_limits<DWORD>::max()) throw std::runtime_error("WAVE is too large for waveOut");
    auto pcm = read_pcm(vfs, asset, 0, static_cast<std::size_t>(asset.data_size));
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM; format.nChannels = asset.channels;
    format.nSamplesPerSec = asset.sample_rate; format.nAvgBytesPerSec = asset.byte_rate;
    format.nBlockAlign = asset.block_align; format.wBitsPerSample = asset.bits_per_sample;
    HWAVEOUT output = nullptr;
    if (waveOutOpen(&output, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        throw std::runtime_error("waveOutOpen failed");
    }
    WAVEHDR header{};
    header.lpData = reinterpret_cast<LPSTR>(pcm.data());
    header.dwBufferLength = static_cast<DWORD>(pcm.size());
    if (waveOutPrepareHeader(output, &header, sizeof(header)) != MMSYSERR_NOERROR) {
        waveOutClose(output); throw std::runtime_error("waveOutPrepareHeader failed");
    }
    if (waveOutWrite(output, &header, sizeof(header)) != MMSYSERR_NOERROR) {
        waveOutUnprepareHeader(output, &header, sizeof(header)); waveOutClose(output);
        throw std::runtime_error("waveOutWrite failed");
    }
    std::uint64_t last_reported = 0; std::exception_ptr callback_error;
    while ((header.dwFlags & WHDR_DONE) == 0) {
        if (progress) {
            MMTIME position{}; position.wType = TIME_SAMPLES;
            if (waveOutGetPosition(output, &position, sizeof(position)) == MMSYSERR_NOERROR &&
                position.wType == TIME_SAMPLES) {
                const auto completed = std::min<std::uint64_t>(position.u.sample, asset.frame_count);
                if (completed > last_reported) {
                    try { progress(completed); last_reported = completed; }
                    catch (...) { callback_error = std::current_exception(); waveOutReset(output); break; }
                }
            }
        }
        Sleep(10);
    }
    if (!callback_error && progress && last_reported < asset.frame_count) {
        try { progress(asset.frame_count); }
        catch (...) { callback_error = std::current_exception(); waveOutReset(output); }
    }
    waveOutUnprepareHeader(output, &header, sizeof(header)); waveOutClose(output);
    if (callback_error) std::rethrow_exception(callback_error);
#else
    (void)vfs; (void)asset; (void)progress;
    throw std::runtime_error("audio playback backend is unavailable on this platform");
#endif
}
} // namespace rb4pc::runtime
