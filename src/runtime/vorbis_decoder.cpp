#include "rb4pc/runtime/vorbis_decoder.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "stb_vorbis.c"

namespace rb4pc::runtime {

VorbisPcmPreview decode_vorbis_preview(const std::filesystem::path& path,
                                       std::uintmax_t offset,
                                       std::size_t maximum_frames) {
    if (maximum_frames == 0) throw std::runtime_error("Vorbis preview must request at least one frame");
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Vorbis path is not a file");
    const auto file_size = std::filesystem::file_size(path);
    if (offset >= file_size) throw std::runtime_error("Vorbis offset is outside the file");
    const auto payload_size = file_size - offset;
    if (payload_size > static_cast<std::uintmax_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("Vorbis payload is too large for the decoder");

    std::vector<unsigned char> encoded(static_cast<std::size_t>(payload_size));
    std::ifstream input(path, std::ios::binary);
    input.seekg(static_cast<std::streamoff>(offset));
    input.read(reinterpret_cast<char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
    if (input.gcount() != static_cast<std::streamsize>(encoded.size()))
        throw std::runtime_error("could not read the Vorbis payload");

    int error = VORBIS__no_error;
    auto* decoder = stb_vorbis_open_memory(encoded.data(), static_cast<int>(encoded.size()), &error, nullptr);
    if (decoder == nullptr) throw std::runtime_error("Vorbis decoder rejected the payload: " + std::to_string(error));

    const auto info = stb_vorbis_get_info(decoder);
    if (info.channels <= 0 || info.sample_rate == 0) {
        stb_vorbis_close(decoder);
        throw std::runtime_error("Vorbis decoder reported invalid stream properties");
    }
    if (maximum_frames > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(info.channels)) {
        stb_vorbis_close(decoder);
        throw std::runtime_error("Vorbis preview size overflows address space");
    }

    const auto sample_capacity = maximum_frames * static_cast<std::size_t>(info.channels);
    if (sample_capacity > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        stb_vorbis_close(decoder);
        throw std::runtime_error("Vorbis preview exceeds the decoder sample limit");
    }
    std::vector<float> samples(sample_capacity);
    const auto frames = stb_vorbis_get_samples_float_interleaved(
        decoder, info.channels, samples.data(), static_cast<int>(samples.size()));
    stb_vorbis_close(decoder);
    if (frames < 0) throw std::runtime_error("Vorbis decoder returned a negative frame count");

    const auto decoded_samples = static_cast<std::size_t>(frames) * static_cast<std::size_t>(info.channels);
    float peak = 0.0F;
    bool finite = true;
    for (std::size_t index = 0; index < decoded_samples; ++index) {
        finite = finite && std::isfinite(samples[index]);
        peak = std::max(peak, std::abs(samples[index]));
    }
    return {static_cast<std::uint32_t>(info.channels), info.sample_rate,
            static_cast<std::size_t>(frames), peak, finite};
}

} // namespace rb4pc::runtime
