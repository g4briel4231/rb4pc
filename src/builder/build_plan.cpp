#include "rb4pc/builder/build_plan.hpp"
#include "rb4pc/inspector/file_inspector.hpp"
#include "rb4pc/inspector/payload_catalog.hpp"
#include "rb4pc/builder/song_layout.hpp"

#include <sstream>

namespace rb4pc::builder {
BuildPlan create_build_plan(const inspector::HeaderSetReport& report) {
    BuildPlan plan;
    plan.platform = report.platform_hint;
    plan.hdr_version = report.metadata.version;
    plan.archive_count = report.archives.size();
    plan.record_count = report.metadata.file_records.size();
    plan.hash_index_validated = report.metadata.hash_index_validated;
    for (const auto& archive : report.archives) {
        if (archive.exists) ++plan.present_archives;
        if (archive.size_matches) ++plan.exact_size_archives;
        if (archive.exists && archive.actual_size >= archive.declared_size) ++plan.usable_size_archives;
    }
    for (const auto& location : report.record_locations) if (location) ++plan.resolved_records;
    for (const auto& record : report.metadata.file_records) if (song_output_path(record.path)) ++plan.song_records;
    plan.ready = plan.hdr_version == 10 && plan.hash_index_validated && plan.archive_count != 0 &&
                 plan.usable_size_archives == plan.archive_count && plan.resolved_records == plan.record_count;
    return plan;
}

BuildPlan create_build_plan(const inspector::HeaderSetReport& report,
                            const inspector::PayloadCatalog& catalog) {
    auto plan = create_build_plan(report);
    plan.convertible_riff_records = catalog.riff_valid_records;
    const auto wave = catalog.riff_form_types.find("WAVE");
    const auto fev = catalog.riff_form_types.find("FEV ");
    if (wave != catalog.riff_form_types.end()) plan.wave_records = wave->second;
    if (fev != catalog.riff_form_types.end()) plan.fev_records = fev->second;
    return plan;
}

std::string format_json(const BuildPlan& plan) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n"
           << "  \"platform\": \"" << plan.platform << "\",\n"
           << "  \"hdrVersion\": " << plan.hdr_version << ",\n"
           << "  \"archives\": {\"total\": " << plan.archive_count
           << ", \"present\": " << plan.present_archives
           << ", \"exactSize\": " << plan.exact_size_archives
           << ", \"usableSize\": " << plan.usable_size_archives << "},\n"
           << "  \"records\": {\"total\": " << plan.record_count
           << ", \"resolved\": " << plan.resolved_records << "},\n"
           << "  \"hashIndexValidated\": " << (plan.hash_index_validated ? "true" : "false") << ",\n"
           << "  \"conversionInventory\": {\"validatedRiff\": " << plan.convertible_riff_records
           << ", \"wave\": " << plan.wave_records << ", \"fev\": " << plan.fev_records << "},\n"
           << "  \"songLayout\": {\"root\": \"songs\", \"records\": " << plan.song_records << "},\n"
           << "  \"readyForConversionPlanning\": " << (plan.ready ? "true" : "false") << "\n}\n";
    return output.str();
}
} // namespace rb4pc::builder
