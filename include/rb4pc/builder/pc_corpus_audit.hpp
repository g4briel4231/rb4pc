#pragma once

#include "rb4pc/builder/pc_asset_catalog.hpp"
#include "rb4pc/builder/pc_audio_catalog.hpp"
#include "rb4pc/builder/pc_character_catalog.hpp"
#include "rb4pc/builder/pc_metadata_catalog.hpp"

#include <filesystem>
#include <string>

namespace rb4pc::builder {

struct PcCorpusAudit {
    PcAssetCatalog assets;
    PcAudioCatalog audio;
    PcCharacterCatalog characters;
    PcMetadataCatalog metadata;

    [[nodiscard]] bool consistent_header_counts() const;
    [[nodiscard]] bool complete() const;
};

struct PcCorpusAuditPublication {
    std::filesystem::path directory;
    PcCorpusAudit audit;
};

[[nodiscard]] PcCorpusAudit audit_pc_corpus(const std::filesystem::path& source);
[[nodiscard]] std::string format_summary_json(const PcCorpusAudit& audit);
[[nodiscard]] PcCorpusAuditPublication write_pc_corpus_audit(
    const std::filesystem::path& source,
    const std::filesystem::path& destination);

} // namespace rb4pc::builder