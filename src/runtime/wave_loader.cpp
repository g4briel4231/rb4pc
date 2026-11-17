#include "rb4pc/runtime/wave_loader.hpp"

#include <stdexcept>

namespace rb4pc::runtime {
namespace {
std::uint16_t u16(const std::vector<std::byte>& b, std::size_t p) {
    return static_cast<std::uint16_t>(std::to_integer<unsigned int>(b[p]) |
        (std::to_integer<unsigned int>(b[p + 1]) << 8));
}
std::uint32_t u32(const std::vector<std::byte>& b, std::size_t p) {
    return static_cast<std::uint32_t>(u16(b, p)) | (static_cast<std::uint32_t>(u16(b, p + 2)) << 16);
}
std::string fourcc(const std::vector<std::byte>& b, std::size_t p) {
    return {reinterpret_cast<const char*>(b.data() + p), 4};
}
} // namespace

WaveAsset load_wave(const ArchiveFileSystem& vfs, std::string_view path) {
    const auto record_size = vfs.file_size(path);
    if (!record_size || *record_size < 12) throw std::runtime_error("WAVE record is missing or too short");
    const auto header = vfs.read(path, 0, 12);
    if (fourcc(header, 0) != "RIFF" || fourcc(header, 8) != "WAVE") throw std::runtime_error("record is not RIFF/WAVE");
    const auto logical_size = static_cast<std::uint64_t>(u32(header, 4)) + 8;
    if (logical_size > *record_size || logical_size < 12) throw std::runtime_error("invalid WAVE logical size");
    WaveAsset asset; asset.path = path;
    bool have_format = false, have_data = false;
    std::uint64_t position = 12;
    while (position < logical_size) {
        if (logical_size - position < 8) throw std::runtime_error("truncated WAVE chunk header");
        const auto chunk = vfs.read(path, position, 8);
        const auto id = fourcc(chunk, 0);
        const auto size = static_cast<std::uint64_t>(u32(chunk, 4));
        if (size + (size & 1U) > logical_size - position - 8) throw std::runtime_error("WAVE chunk exceeds logical size");
        if (id == "fmt " && size >= 16 && !have_format) {
            const auto fmt = vfs.read(path, position + 8, 16);
            asset.format_tag = u16(fmt, 0); asset.channels = u16(fmt, 2);
            asset.sample_rate = u32(fmt, 4); asset.byte_rate = u32(fmt, 8);
            asset.block_align = u16(fmt, 12); asset.bits_per_sample = u16(fmt, 14);
            have_format = true;
        } else if (id == "data" && !have_data) {
            asset.data_offset = position + 8; asset.data_size = size; have_data = true;
        }
        position += 8 + size + (size & 1U);
    }
    if (!have_format || !have_data || asset.format_tag != 1 || asset.channels == 0 ||
        asset.sample_rate == 0 || asset.block_align == 0) throw std::runtime_error("unsupported or incomplete WAVE metadata");
    asset.frame_count = asset.data_size / asset.block_align;
    asset.duration_seconds = static_cast<double>(asset.frame_count) / asset.sample_rate;
    return asset;
}

std::vector<std::byte> read_pcm(const ArchiveFileSystem& vfs, const WaveAsset& asset,
                                std::uint64_t offset, std::size_t size) {
    if (offset > asset.data_size || size > asset.data_size - offset) throw std::runtime_error("PCM read exceeds data chunk");
    return vfs.read(asset.path, asset.data_offset + offset, size);
}
} // namespace rb4pc::runtime
