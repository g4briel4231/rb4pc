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

struct TextureFamilyEvidence {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mipmaps = 0;
    std::uint32_t header_bytes = 0;
    std::uint32_t mipmap_entry_bytes = 0;
    std::uint32_t marker = 0;
    std::uint32_t bits_per_pixel = 0;
    std::uint64_t files = 0;
};

struct TextureEvidenceReport {
    std::filesystem::path root;
    std::uint64_t candidates = 0;
    std::uint64_t magic4_version8 = 0;
    std::uint64_t other_families = 0;
    std::uint64_t unreadable = 0;
    std::vector<TextureFamilyEvidence> families;
};

struct TextureCoverageRow {
    std::uint32_t magic = 0;
    std::uint32_t platform_version = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mipmaps = 0;
    std::uint32_t bits_per_pixel = 0;
    std::string state;
    std::string reason;
    std::uint64_t files = 0;
};

struct TextureCoverageReport {
    std::filesystem::path root;
    std::uint64_t candidates = 0;
    std::uint64_t convertible = 0;
    std::uint64_t read_only = 0;
    std::uint64_t invalid = 0;
    std::vector<TextureCoverageRow> rows;
};

[[nodiscard]] TexturePairReport compare_texture_pair(
    const std::filesystem::path& source, const std::filesystem::path& reference);
[[nodiscard]] std::string format_json(const TexturePairReport& report);
[[nodiscard]] TextureDirectoryPairReport compare_texture_directories(
    const std::filesystem::path& source_root, const std::filesystem::path& reference_root);
[[nodiscard]] std::string format_json(const TextureDirectoryPairReport& report);
[[nodiscard]] TextureEvidenceReport profile_magic4_version8(const std::filesystem::path& root);
[[nodiscard]] std::string format_json(const TextureEvidenceReport& report);
[[nodiscard]] TextureCoverageReport build_texture_coverage_matrix(const std::filesystem::path& root);
[[nodiscard]] std::string format_json(const TextureCoverageReport& report);
} // namespace rb4pc::builder