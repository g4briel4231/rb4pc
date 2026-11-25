#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

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

[[nodiscard]] TexturePairReport compare_texture_pair(
    const std::filesystem::path& source, const std::filesystem::path& reference);
[[nodiscard]] std::string format_json(const TexturePairReport& report);
} // namespace rb4pc::builder