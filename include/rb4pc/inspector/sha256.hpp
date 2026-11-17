#pragma once

#include <filesystem>
#include <string>

namespace rb4pc::inspector {

[[nodiscard]] std::string sha256_file(const std::filesystem::path& path);

} // namespace rb4pc::inspector

