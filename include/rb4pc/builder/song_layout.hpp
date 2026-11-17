#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace rb4pc::builder {
[[nodiscard]] std::optional<std::filesystem::path> song_output_path(std::string_view internal_path);
} // namespace rb4pc::builder
