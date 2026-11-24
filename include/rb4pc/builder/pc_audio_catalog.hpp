#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace rb4pc::builder {

struct PcAudioCatalog {
    std::size_t candidate_headers = 0;
    std::size_t valid_headers = 0;
    std::size_t invalid_headers = 0;
    std::size_t audio_records = 0;
    std::uint64_t audio_bytes = 0;
    std::size_t mogg_records = 0;
    std::size_t mogg_version_10 = 0;
    std::size_t mogg_embedded_ogg = 0;
    std::size_t mogg_unsupported_or_protected = 0;
    std::size_t wave_records = 0;
    std::size_t riff_wave = 0;
    std::size_t bank_records = 0;
    std::size_t riff_fev = 0;
    std::size_t opaque_records = 0;
    std::size_t unreadable_records = 0;
    std::map<std::string, std::size_t> mogg_versions;
    std::map<std::string, std::size_t> invalid_reasons;
    std::vector<std::string> errors;
};

[[nodiscard]] PcAudioCatalog catalog_pc_audio(const std::filesystem::path& source);
[[nodiscard]] std::string format_json(const PcAudioCatalog& catalog);

} // namespace rb4pc::builder
