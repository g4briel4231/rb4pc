#pragma once

#include <cstdint>
#include <filesystem>

namespace rb4pc::runtime {

struct AppConfig {
    float master_volume = 1.0F;
    std::int32_t latency_ms = 0;
};

[[nodiscard]] AppConfig load_app_config(const std::filesystem::path& path);
[[nodiscard]] AppConfig load_or_create_app_config(const std::filesystem::path& path);
void save_app_config(const std::filesystem::path& path, const AppConfig& config);

} // namespace rb4pc::runtime
