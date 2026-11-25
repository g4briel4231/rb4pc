#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rb4pc::builder {
enum class TextureConversionState { convertible, read_only, invalid };

struct TextureConversionPlanEntry {
    std::filesystem::path relative_path;
    TextureConversionState state = TextureConversionState::invalid;
    std::string reason;
    std::uint32_t magic = 0;
    std::uint32_t platform_version = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mipmap_count = 0;
    std::uint32_t bits_per_pixel = 0;
};

struct TextureConversionPlan {
    std::filesystem::path root;
    std::vector<TextureConversionPlanEntry> entries;
    std::uint64_t convertible = 0;
    std::uint64_t read_only = 0;
    std::uint64_t invalid = 0;
};

struct TextureBatchItem {
    std::filesystem::path source_relative_path;
    std::filesystem::path destination_relative_path;
    std::uint32_t bits_per_pixel = 0;
    std::string source_sha256;
    std::string destination_sha256;
    std::string payload_sha256;
};

struct TextureBatchResult {
    std::filesystem::path directory;
    std::filesystem::path summary;
    std::vector<TextureBatchItem> items;
    std::uint64_t skipped_read_only = 0;
    std::uint64_t skipped_invalid = 0;
};
struct TextureBatchVerificationEntry {
    std::filesystem::path source_relative_path;
    std::filesystem::path destination_relative_path;
    bool verified = false;
    std::string reason;
    std::string source_sha256;
    std::string destination_sha256;
    std::string payload_sha256;
};

struct TextureBatchVerification {
    std::filesystem::path source_root;
    std::filesystem::path batch_root;
    std::vector<TextureBatchVerificationEntry> entries;
    std::uint64_t verified = 0;
    std::uint64_t missing = 0;
    std::uint64_t invalid = 0;
    std::uint64_t unexpected = 0;
    [[nodiscard]] bool complete() const {
        return missing == 0 && invalid == 0 && unexpected == 0 && verified == entries.size();
    }
};

[[nodiscard]] TextureConversionPlan plan_texture_conversions(const std::filesystem::path& root);
[[nodiscard]] std::filesystem::path pc_texture_relative_path(const std::filesystem::path& source_relative_path);
[[nodiscard]] TextureBatchResult convert_texture_batch(
    const std::filesystem::path& source_root, const std::filesystem::path& destination_root);
[[nodiscard]] std::string format_json(const TextureConversionPlan& plan);
[[nodiscard]] std::string format_json(const TextureBatchResult& result);
[[nodiscard]] TextureBatchVerification verify_texture_batch(
    const std::filesystem::path& source_root, const std::filesystem::path& batch_root);
[[nodiscard]] std::string format_json(const TextureBatchVerification& verification);
} // namespace rb4pc::builder