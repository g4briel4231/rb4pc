#include "rb4pc/builder/build_plan.hpp"
#include "rb4pc/inspector/file_inspector.hpp"

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
    plan.ready = plan.hdr_version == 10 && plan.hash_index_validated && plan.archive_count != 0 &&
                 plan.usable_size_archives == plan.archive_count && plan.resolved_records == plan.record_count;
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
           << "  \"readyForConversionPlanning\": " << (plan.ready ? "true" : "false") << "\n}\n";
    return output.str();
}
} // namespace rb4pc::builder
