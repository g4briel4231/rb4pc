#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rb4pc::builder {
struct TexturePairReport {
    std::filesystem::path source;
    std::filesystem::path reference;
    std::uint32_t source_magic = 0;
    std::uint32_t source_version = 0;
    std::uint32_t reference_magic = 0;
    std::uint32_t reference_version = 0;
    std::uint32_t source_mipmaps = 0;
    std::uint32_t reference_mipmaps = 0;
    std::string source_payload_sha256;
    std::string reference_payload_sha256;
    bool payload_compatible = false;
    std::string reason;
};

struct TextureDirectoryPairReport {
    std::filesystem::path source_root;
    std::filesystem::path reference_root;
    std::vector<TexturePairReport> pairs;
    std::vector<std::filesystem::path> unmatched_sources;
    std::vector<std::filesystem::path> unmatched_references;
    std::uint64_t compatible = 0;
    std::uint64_t incompatible = 0;
};

[[nodiscard]] TexturePairReport compare_texture_pair(
    const std::filesystem::path& source, const std::filesystem::path& reference);
[[nodiscard]] std::string format_json(const TexturePairReport& report);
[[nodiscard]] TextureDirectoryPairReport compare_texture_directories(
    const std::filesystem::path& source_root, const std::filesystem::path& reference_root);
[[nodiscard]] std::string format_json(const TextureDirectoryPairReport& report);
} // namespace rb4pc::builder