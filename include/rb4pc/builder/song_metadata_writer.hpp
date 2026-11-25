#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
namespace rb4pc::builder {
struct CanonicalSongMetadata {
    std::string short_name;
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::optional<std::int32_t> year;
    std::optional<std::uint32_t> preview_start_ms;
    bool operator==(const CanonicalSongMetadata&) const = default;
};
struct SongMetadataConversionResult {
    std::filesystem::path destination;
    std::filesystem::path manifest;
    CanonicalSongMetadata metadata;
    std::string source_sha256;
    std::string destination_sha256;
};
[[nodiscard]] CanonicalSongMetadata parse_song_ini(const std::filesystem::path& path);
[[nodiscard]] CanonicalSongMetadata read_canonical_song_metadata(const std::filesystem::path& path);
[[nodiscard]] std::string format_json(const CanonicalSongMetadata& metadata);
[[nodiscard]] SongMetadataConversionResult convert_song_ini_metadata(
    const std::filesystem::path& source, const std::filesystem::path& destination);
} // namespace rb4pc::builder