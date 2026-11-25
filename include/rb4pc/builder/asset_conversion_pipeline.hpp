#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
namespace rb4pc::builder {
struct AssetConversionPipelineResult {
    std::filesystem::path directory;
    std::filesystem::path summary;
    std::uint64_t converted_textures = 0;
    std::uint64_t extracted_audio = 0;
    std::uint64_t skipped_texture_read_only = 0;
    std::uint64_t skipped_texture_invalid = 0;
    std::uint64_t skipped_audio_runtime_ready = 0;
    std::uint64_t skipped_audio_research_only = 0;
    std::uint64_t skipped_audio_invalid = 0;
};
[[nodiscard]] AssetConversionPipelineResult convert_asset_pipeline(
    const std::filesystem::path& source_root, const std::filesystem::path& destination_root);
[[nodiscard]] std::string format_json(const AssetConversionPipelineResult& result);
} // namespace rb4pc::builder