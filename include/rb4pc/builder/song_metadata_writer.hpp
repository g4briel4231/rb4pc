#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

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

struct NativeMoggsongMetadata {
    std::string mogg_path;
    std::string midi_path;
    bool operator==(const NativeMoggsongMetadata&) const = default;
};

struct NativeMoggsongWriteResult {
    std::filesystem::path destination;
    std::filesystem::path manifest;
    NativeMoggsongMetadata metadata;
    std::string destination_sha256;
};
struct SongMetadataBatchItem {
    std::filesystem::path source_relative_path;
    std::filesystem::path metadata_relative_path;
    std::filesystem::path manifest_relative_path;
    CanonicalSongMetadata metadata;
    std::string source_sha256;
    std::string destination_sha256;
};

struct SongMetadataBatchResult {
    std::filesystem::path directory;
    std::filesystem::path summary;
    std::vector<SongMetadataBatchItem> items;
};

[[nodiscard]] CanonicalSongMetadata parse_song_ini(const std::filesystem::path& path);
[[nodiscard]] CanonicalSongMetadata read_canonical_song_metadata(const std::filesystem::path& path);
[[nodiscard]] std::string format_json(const CanonicalSongMetadata& metadata);
[[nodiscard]] std::string format_json(const SongMetadataBatchResult& result);
[[nodiscard]] NativeMoggsongMetadata read_native_moggsong_metadata(const std::filesystem::path& path);
[[nodiscard]] NativeMoggsongWriteResult write_native_moggsong_metadata(
    const NativeMoggsongMetadata& metadata, const std::filesystem::path& destination);[[nodiscard]] SongMetadataConversionResult convert_song_ini_metadata(
    const std::filesystem::path& source, const std::filesystem::path& destination);
[[nodiscard]] SongMetadataBatchResult convert_song_metadata_batch(
    const std::filesystem::path& source_root, const std::filesystem::path& destination_root);
} // namespace rb4pc::builder