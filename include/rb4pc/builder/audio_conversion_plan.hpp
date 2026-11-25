#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace rb4pc::builder {
enum class AudioPlanState { runtime_ready, extraction_ready, research_only, invalid };
struct AudioPlanEntry {
    std::filesystem::path relative_path;
    AudioPlanState state = AudioPlanState::invalid;
    std::string kind;
    std::string reason;
    std::uint32_t container_version = 0;
    std::uint64_t size = 0;
};
struct AudioConversionPlan {
    std::filesystem::path root;
    std::vector<AudioPlanEntry> entries;
    std::uint64_t runtime_ready = 0;
    std::uint64_t extraction_ready = 0;
    std::uint64_t research_only = 0;
    std::uint64_t invalid = 0;
};
struct AudioExtractionResult {
    std::filesystem::path destination;
    std::filesystem::path manifest;
    std::uint32_t mogg_version = 0;
    std::uint64_t ogg_offset = 0;
    std::uint64_t extracted_bytes = 0;
    std::string source_sha256;
    std::string destination_sha256;
};
[[nodiscard]] AudioExtractionResult extract_embedded_ogg(
    const std::filesystem::path& source, const std::filesystem::path& destination);

[[nodiscard]] AudioConversionPlan plan_audio_conversions(const std::filesystem::path& root);
[[nodiscard]] std::string format_json(const AudioConversionPlan& plan);
} // namespace rb4pc::builder