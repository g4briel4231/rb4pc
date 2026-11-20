#pragma once

#include <filesystem>
#include <istream>
#include <cstdint>
#include <string>

namespace rb4pc::inspector {

[[nodiscard]] std::string sha256_file(const std::filesystem::path& path);
[[nodiscard]] std::string sha256_range(std::istream& input, std::uint64_t offset, std::uint64_t size);

} // namespace rb4pc::inspector

