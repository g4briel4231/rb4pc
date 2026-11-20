#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rb4pc::inspector {
struct TextureMipmap {
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t flags = 0;
    std::uint64_t data_offset = 0;
    std::uint32_t data_size = 0;
    std::string sha256;
};

struct TextureContainer {
    std::filesystem::path path;
    std::uint32_t magic = 0;
    std::uint32_t platform_version = 0;
    std::uint32_t marker = 0;
    std::uint32_t header_data_size = 0;
    std::uint32_t mipmap_entry_size = 0;
    std::uint32_t footer_size = 0;
    bool individual_mipmap_payloads = true;
    std::uint64_t payload_offset = 0;
    std::uint64_t payload_size = 0;
    std::string payload_sha256;
    std::vector<TextureMipmap> mipmaps;
};

struct TextureCompatibility {
    bool compatible = false;
    std::string reason;
};

struct TextureCorpusFamily {
    std::uint32_t magic = 0;
    std::uint32_t platform_version = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t flags = 0;
    std::uint32_t mipmap_count = 0;
    std::uint32_t marker = 0;
    std::uint32_t bits_per_pixel = 0;
    std::uint64_t file_count = 0;
};

struct TextureRejectedSignature {
    std::uint32_t magic = 0;
    std::uint32_t platform_version = 0;
    std::uint64_t file_count = 0;
};

struct TextureCorpus {
    std::filesystem::path root;
    std::string extension;
    std::uint64_t candidate_count = 0;
    std::uint64_t parsed_count = 0;
    std::vector<TextureCorpusFamily> families;
    std::vector<TextureRejectedSignature> rejected_signatures;
};

[[nodiscard]] TextureContainer inspect_texture_container(const std::filesystem::path& path);
[[nodiscard]] TextureCompatibility compare_texture_payloads(
    const TextureContainer& source, const TextureContainer& reference);
[[nodiscard]] std::string format_texture_report(const TextureContainer& texture);
[[nodiscard]] TextureCorpus inspect_texture_corpus(
    const std::filesystem::path& root, std::string extension);
[[nodiscard]] std::string format_texture_corpus_report(const TextureCorpus& corpus);
} // namespace rb4pc::inspector
