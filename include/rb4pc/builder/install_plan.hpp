#pragma once
#include "rb4pc/builder/custom_song_catalog.hpp"

namespace rb4pc::builder {
struct InstallOperation { std::filesystem::path source, destination; std::uintmax_t size = 0; bool collision = false; };
struct InstallPlan { std::filesystem::path target; std::vector<InstallOperation> operations; std::size_t collisions = 0; std::uintmax_t total_bytes = 0; };
[[nodiscard]] InstallPlan create_install_plan(const CustomSongCatalog& catalog, const std::filesystem::path& target);
[[nodiscard]] std::string format_json(const InstallPlan& plan);
} // namespace rb4pc::builder
