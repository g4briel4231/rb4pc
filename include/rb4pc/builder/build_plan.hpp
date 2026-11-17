#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace rb4pc::inspector { struct HeaderSetReport; }
namespace rb4pc::inspector { struct PayloadCatalog; }

namespace rb4pc::builder {
struct BuildPlan {
    std::string platform;
    std::uint32_t hdr_version = 0;
    std::size_t archive_count = 0;
    std::size_t present_archives = 0;
    std::size_t exact_size_archives = 0;
    std::size_t usable_size_archives = 0;
    std::size_t record_count = 0;
    std::size_t resolved_records = 0;
    bool hash_index_validated = false;
    bool ready = false;
    std::size_t convertible_riff_records = 0;
    std::size_t wave_records = 0;
    std::size_t fev_records = 0;
    std::size_t song_records = 0;
};
[[nodiscard]] BuildPlan create_build_plan(const inspector::HeaderSetReport& report);
[[nodiscard]] BuildPlan create_build_plan(
    const inspector::HeaderSetReport& report, const inspector::PayloadCatalog& catalog);
[[nodiscard]] std::string format_json(const BuildPlan& plan);
} // namespace rb4pc::builder
