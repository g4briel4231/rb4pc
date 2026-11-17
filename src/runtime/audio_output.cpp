#include "rb4pc/runtime/audio_output.hpp"

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
    while ((header.dwFlags & WHDR_DONE) == 0) Sleep(10);
    waveOutUnprepareHeader(output, &header, sizeof(header)); waveOutClose(output);
#else
    (void)vfs; (void)asset;
    throw std::runtime_error("audio playback backend is unavailable on this platform");
#endif
}
} // namespace rb4pc::runtime
