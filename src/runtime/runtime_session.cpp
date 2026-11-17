#include "rb4pc/runtime/runtime_session.hpp"

#include "rb4pc/inspector/payload_catalog.hpp"
#include "rb4pc/runtime/audio_output.hpp"
#include "rb4pc/runtime/input.hpp"

#include <utility>
#include <sstream>

namespace rb4pc::runtime {
RuntimeSession::RuntimeSession(builder::BuildPlan plan, ArchiveFileSystem vfs)
    : plan_(std::move(plan)), vfs_(std::move(vfs)) {}

RuntimeSession RuntimeSession::boot(const std::filesystem::path& header) {
    return from_report(inspector::inspect_header_set(header));
}

RuntimeSession RuntimeSession::from_report(inspector::HeaderSetReport report) {
    const auto catalog = inspector::catalog_payloads(report);
    auto plan = builder::create_build_plan(report, catalog);
    return RuntimeSession(std::move(plan), ArchiveFileSystem(std::move(report)));
}

const builder::BuildPlan& RuntimeSession::plan() const { return plan_; }
const ArchiveFileSystem& RuntimeSession::vfs() const { return vfs_; }

std::vector<WaveAsset> RuntimeSession::discover_wave_assets(std::string_view prefix, std::size_t limit) const {
    std::vector<WaveAsset> result;
    if (limit == 0) return result;
    for (const auto& path : vfs_.list_paths(prefix, vfs_.file_count())) {
        if (!path.ends_with(".wav")) continue;
        try { result.push_back(load_wave(vfs_, path)); } catch (const std::exception&) { continue; }
        if (result.size() == limit) break;
    }
    return result;
}

std::string RuntimeSession::diagnostics_json() const {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n"
           << "  \"sourceReady\": " << (plan_.ready ? "true" : "false") << ",\n"
           << "  \"platform\": \"" << plan_.platform << "\",\n"
           << "  \"mountedRecords\": " << vfs_.file_count() << ",\n"
           << "  \"convertibleRiffRecords\": " << plan_.convertible_riff_records << ",\n"
           << "  \"waveRecords\": " << plan_.wave_records << ",\n"
           << "  \"fevRecords\": " << plan_.fev_records << ",\n"
           << "  \"capabilities\": {\n"
           << "    \"readOnlyVfs\": true,\n"
           << "    \"pcmWaveLoading\": true,\n"
           << "    \"pcmFrameStreaming\": true,\n"
           << "    \"audioPlayback\": " << (audio_playback_available() ? "true" : "false") << ",\n"
           << "    \"controllerInput\": " << (input_backend_available() ? "true" : "false") << ",\n"
           << "    \"rendering\": false,\n"
           << "    \"gameplay\": false\n  }\n}\n";
    return output.str();
}
} // namespace rb4pc::runtime
