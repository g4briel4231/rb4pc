#pragma once
#include <cstdint>
#include <filesystem>
namespace rb4pc::runtime {
struct MoggHeader { std::uint32_t version = 0; std::uint32_t ogg_offset = 0; std::uintmax_t file_size = 0; bool embedded_ogg = false; };
struct OggVorbisInfo {
    std::uint8_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint64_t final_granule = 0;
    std::size_t page_count = 0;
    double duration_seconds = 0.0;
};
[[nodiscard]] MoggHeader parse_mogg_header(const std::filesystem::path& path);
[[nodiscard]] OggVorbisInfo inspect_ogg_vorbis(const std::filesystem::path& path,
                                               std::uintmax_t offset = 0);
void extract_mogg_ogg(const std::filesystem::path& source, const MoggHeader& header,
                      const std::filesystem::path& destination);
} // namespace rb4pc::runtime
