#pragma once

#include "rb4pc/builder/build_plan.hpp"
#include "rb4pc/runtime/archive_file_system.hpp"
#include "rb4pc/runtime/wave_loader.hpp"

namespace rb4pc::runtime {
class RuntimeSession {
public:
    [[nodiscard]] static RuntimeSession boot(const std::filesystem::path& header);
    [[nodiscard]] static RuntimeSession from_report(inspector::HeaderSetReport report);
    [[nodiscard]] const builder::BuildPlan& plan() const;
    [[nodiscard]] const ArchiveFileSystem& vfs() const;
    [[nodiscard]] std::vector<WaveAsset> discover_wave_assets(
        std::string_view prefix, std::size_t limit) const;
private:
    RuntimeSession(builder::BuildPlan plan, ArchiveFileSystem vfs);
    builder::BuildPlan plan_;
    ArchiveFileSystem vfs_;
};
} // namespace rb4pc::runtime
