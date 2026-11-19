#include "rb4pc/runtime/vorbis_decoder.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "stb_vorbis.c"

namespace rb4pc::runtime {

struct VorbisPcmStream::Impl {
    std::vector<unsigned char> encoded;
    stb_vorbis* decoder = nullptr;
    std::uint32_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint64_t total_frames = 0;
    std::uint64_t frame_position = 0;
    ~Impl() { if (decoder != nullptr) stb_vorbis_close(decoder); }
};

std::size_t VorbisPcmBlock::frame_count() const {
    return channels == 0 ? 0 : samples.size() / channels;
}

VorbisPcmStream::VorbisPcmStream(const std::filesystem::path& path, std::uintmax_t offset)
    : impl_(std::make_unique<Impl>()) {
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Vorbis path is not a file");
    const auto file_size = std::filesystem::file_size(path);
    if (offset >= file_size) throw std::runtime_error("Vorbis offset is outside the file");
    const auto payload_size = file_size - offset;
    if (payload_size > static_cast<std::uintmax_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("Vorbis payload is too large for the decoder");

    impl_->encoded.resize(static_cast<std::size_t>(payload_size));
    std::ifstream input(path, std::ios::binary);
    input.seekg(static_cast<std::streamoff>(offset));
    input.read(reinterpret_cast<char*>(impl_->encoded.data()),
               static_cast<std::streamsize>(impl_->encoded.size()));
    if (input.gcount() != static_cast<std::streamsize>(impl_->encoded.size()))
        throw std::runtime_error("could not read the Vorbis payload");

    int error = VORBIS__no_error;
    impl_->decoder = stb_vorbis_open_memory(impl_->encoded.data(),
                                             static_cast<int>(impl_->encoded.size()), &error, nullptr);
    if (impl_->decoder == nullptr)
        throw std::runtime_error("Vorbis decoder rejected the payload: " + std::to_string(error));
    const auto info = stb_vorbis_get_info(impl_->decoder);
    if (info.channels <= 0 || info.sample_rate == 0)
        throw std::runtime_error("Vorbis decoder reported invalid stream properties");
    impl_->channels = static_cast<std::uint32_t>(info.channels);
    impl_->sample_rate = info.sample_rate;
    impl_->total_frames = stb_vorbis_stream_length_in_samples(impl_->decoder);
    if (impl_->total_frames == 0) throw std::runtime_error("Vorbis stream has no decodable frames");
}

VorbisPcmStream::~VorbisPcmStream() = default;
VorbisPcmStream::VorbisPcmStream(VorbisPcmStream&&) noexcept = default;
VorbisPcmStream& VorbisPcmStream::operator=(VorbisPcmStream&&) noexcept = default;
std::uint32_t VorbisPcmStream::channels() const { return impl_->channels; }
std::uint32_t VorbisPcmStream::sample_rate() const { return impl_->sample_rate; }
std::uint64_t VorbisPcmStream::total_frames() const { return impl_->total_frames; }
std::uint64_t VorbisPcmStream::frame_position() const { return impl_->frame_position; }
bool VorbisPcmStream::exhausted() const { return impl_->frame_position >= impl_->total_frames; }

void VorbisPcmStream::seek_frame(std::uint64_t frame) {
    if (frame > impl_->total_frames || frame > std::numeric_limits<unsigned int>::max())
        throw std::runtime_error("Vorbis seek exceeds stream duration");
    if (stb_vorbis_seek_frame(impl_->decoder, static_cast<unsigned int>(frame)) == 0)
        throw std::runtime_error("Vorbis decoder could not seek to the requested frame");
    impl_->frame_position = frame;
}

VorbisPcmBlock VorbisPcmStream::read_frames(std::size_t maximum_frames) {
    if (maximum_frames == 0) throw std::runtime_error("Vorbis stream read must request frames");
    const auto remaining = impl_->total_frames - impl_->frame_position;
    const auto requested = static_cast<std::size_t>(std::min<std::uint64_t>(maximum_frames, remaining));
    if (requested > static_cast<std::size_t>(std::numeric_limits<int>::max()) / impl_->channels)
        throw std::runtime_error("Vorbis stream read exceeds the decoder sample limit");
    VorbisPcmBlock result{impl_->frame_position, impl_->channels, impl_->sample_rate, {}};
    if (requested == 0) return result;
    result.samples.resize(requested * impl_->channels);
    const auto frames = stb_vorbis_get_samples_float_interleaved(
        impl_->decoder, static_cast<int>(impl_->channels), result.samples.data(),
        static_cast<int>(result.samples.size()));
    if (frames < 0) throw std::runtime_error("Vorbis decoder returned a negative frame count");
    result.samples.resize(static_cast<std::size_t>(frames) * impl_->channels);
    impl_->frame_position += static_cast<std::uint64_t>(frames);
    return result;
}

VorbisPcmPreview decode_vorbis_preview(const std::filesystem::path& path,
                                       std::uintmax_t offset,
                                       std::size_t maximum_frames) {
    if (maximum_frames == 0) throw std::runtime_error("Vorbis preview must request at least one frame");
    VorbisPcmStream stream(path, offset);
    const auto block = stream.read_frames(maximum_frames);
    float peak = 0.0F;
    bool finite = true;
    for (const auto sample : block.samples) {
        finite = finite && std::isfinite(sample);
        peak = std::max(peak, std::abs(sample));
    }
    return {stream.channels(), stream.sample_rate(), block.frame_count(), peak, finite};
}

} // namespace rb4pc::runtime
