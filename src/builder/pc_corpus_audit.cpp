#include "rb4pc/builder/pc_corpus_audit.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace rb4pc::builder {
namespace {

void write_json(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("could not create audit file: " + path.string());
    output << content;
    if (!output) throw std::runtime_error("could not finalize audit file: " + path.string());
}

} // namespace

bool PcCorpusAudit::consistent_header_counts() const {
    return assets.candidate_headers == audio.candidate_headers &&
           assets.candidate_headers == characters.candidate_headers &&
           assets.candidate_headers == metadata.candidate_headers &&
           assets.valid_headers == audio.valid_headers &&
           assets.valid_headers == characters.valid_headers &&
           assets.valid_headers == metadata.valid_headers;
}

bool PcCorpusAudit::complete() const {
    return consistent_header_counts() && metadata.evidence_classification_complete() &&
           assets.invalid_headers == 0 && audio.invalid_headers == 0 &&
           audio.unreadable_records == 0 && characters.invalid_headers == 0 &&
           characters.unreadable_records == 0 && metadata.invalid_headers == 0 &&
           metadata.unreadable_records == 0 && assets.valid_headers > 0 &&
           audio.valid_headers > 0 && characters.valid_headers > 0 && metadata.valid_headers > 0;
}

PcCorpusAudit audit_pc_corpus(const std::filesystem::path& source) {
    return {catalog_pc_assets(source), catalog_pc_audio(source), catalog_pc_characters(source),
            catalog_pc_metadata(source)};
}

std::string format_summary_json(const PcCorpusAudit& audit) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 5,\n"
           << "  \"complete\": " << (audit.complete() ? "true" : "false") << ",\n"
           << "  \"consistentHeaderCounts\": " << (audit.consistent_header_counts() ? "true" : "false") << ",\n"
           << "  \"metadataEvidenceComplete\": "
           << (audit.metadata.evidence_classification_complete() ? "true" : "false") << ",\n"
           << "  \"metadataBoundedOffsetRegionsComplete\": "
           << (audit.metadata.bounded_offset_region_classification_complete() ? "true" : "false") << ",\n"
           << "  \"headers\": {\"assets\": " << audit.assets.valid_headers
           << ", \"audio\": " << audit.audio.valid_headers
           << ", \"characters\": " << audit.characters.valid_headers
           << ", \"metadata\": " << audit.metadata.valid_headers << "},\n"
           << "  \"assets\": {\"records\": " << audit.assets.records
           << ", \"payloadBytes\": " << audit.assets.payload_bytes
           << ", \"nativePaths\": " << audit.assets.native_path_records
           << ", \"intermediatePaths\": " << audit.assets.intermediate_path_records
           << ", \"unknownPaths\": " << audit.assets.unknown_path_records << "},\n"
           << "  \"audio\": {\"records\": " << audit.audio.audio_records
           << ", \"bytes\": " << audit.audio.audio_bytes
           << ", \"moggEmbeddedOgg\": " << audit.audio.mogg_embedded_ogg
           << ", \"riffWave\": " << audit.audio.riff_wave
           << ", \"riffFev\": " << audit.audio.riff_fev
           << ", \"unreadable\": " << audit.audio.unreadable_records << "},\n"
           << "  \"characters\": {\"records\": " << audit.characters.records
           << ", \"bytes\": " << audit.characters.bytes
           << ", \"families\": " << audit.characters.families.size()
           << ", \"unreadable\": " << audit.characters.unreadable_records << "},\n"
           << "  \"metadata\": {\"records\": " << audit.metadata.records
           << ", \"bytes\": " << audit.metadata.bytes
           << ", \"families\": " << audit.metadata.families.size()
           << ", \"profiledFields\": " << audit.metadata.profiled_fields()
           << ", \"classifiedFields\": " << audit.metadata.classified_fields()
           << ", \"evidenceClassificationComplete\": "
           << (audit.metadata.evidence_classification_complete() ? "true" : "false")
           << ", \"unreadable\": " << audit.metadata.unreadable_records << "},\n"
           << "  \"files\": [\"summary.json\", \"assets.json\", \"audio.json\", \"characters.json\", \"metadata.json\"]\n}\n";
    return output.str();
}

PcCorpusAuditPublication write_pc_corpus_audit(
    const std::filesystem::path& source,
    const std::filesystem::path& destination) {
    if (std::filesystem::exists(destination))
        throw std::runtime_error("audit destination already exists: " + destination.string());
    auto audit = audit_pc_corpus(source);
    auto temporary = destination;
    temporary += ".rb4pc-tmp";
    if (std::filesystem::exists(temporary))
        throw std::runtime_error("temporary audit destination already exists: " + temporary.string());
    try {
        std::filesystem::create_directories(temporary);
        write_json(temporary / "summary.json", format_summary_json(audit));
        write_json(temporary / "assets.json", format_json(audit.assets));
        write_json(temporary / "audio.json", format_json(audit.audio));
        write_json(temporary / "characters.json", format_json(audit.characters));
        write_json(temporary / "metadata.json", format_json(audit.metadata));
        std::filesystem::rename(temporary, destination);
    } catch (...) {
        std::error_code error;
        std::filesystem::remove_all(temporary, error);
        throw;
    }
    return {destination, std::move(audit)};
}

} // namespace rb4pc::builder