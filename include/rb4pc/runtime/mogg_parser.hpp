#pragma once
#include <cstdint>
#include <filesystem>
namespace rb4pc::runtime {
struct MoggHeader { std::uint32_t version = 0; std::uint32_t ogg_offset = 0; std::uintmax_t file_size = 0; bool embedded_ogg = false; };
[[nodiscard]] MoggHeader parse_mogg_header(const std::filesystem::path& path);
void extract_mogg_ogg(const std::filesystem::path& source, const MoggHeader& header,
                      const std::filesystem::path& destination);
} // namespace rb4pc::runtime
