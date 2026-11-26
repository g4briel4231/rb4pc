#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace rb4pc::builder {
struct NativeSongData {
    std::uint32_t type{};
    std::uint32_t song_id{};
    std::int16_t version{};
    std::string game_origin;
    float preview_start{};
    float preview_end{};
    std::string name;
    std::string artist;
    std::string album_name;
    std::int16_t album_track_number{};
    std::int32_t album_year{};
    std::int32_t original_year{};
    std::string genre;
    float song_length{};
    float guitar_rank{};
    float bass_rank{};
    float vocals_rank{};
    float drum_rank{};
    float band_rank{};
    float keys_rank{};
    float real_keys_rank{};
    bool tutorial{};
    bool album_art{};
    bool cover{};
    std::uint8_t vocal_gender{};
    std::string medium;
    bool has_freestyle_vocals{};
    std::int32_t vocal_parts{};
    std::int32_t flags{};
    bool fake{};
    std::string short_name;
    bool operator==(const NativeSongData&) const = default;
};

struct NativeSongDataWriteResult {
    std::filesystem::path destination;
    std::filesystem::path manifest;
    NativeSongData metadata;
    std::string destination_sha256;
};

[[nodiscard]] NativeSongData read_native_songdata(const std::filesystem::path& path);
[[nodiscard]] NativeSongDataWriteResult write_native_songdata(
    const NativeSongData& metadata, const std::filesystem::path& destination);
[[nodiscard]] NativeSongDataWriteResult roundtrip_native_songdata(
    const std::filesystem::path& source, const std::filesystem::path& destination);
[[nodiscard]] std::string format_json(const NativeSongData& metadata);
} // namespace rb4pc::builder