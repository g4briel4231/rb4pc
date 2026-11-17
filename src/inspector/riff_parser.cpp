#include "rb4pc/inspector/riff_parser.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace rb4pc::inspector {
namespace {

std::uint32_t read_u32_le(const unsigned char* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

bool read_at(std::istream& input, std::uint64_t offset, unsigned char* output, std::size_t size) {
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) return false;
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) return false;
    input.read(reinterpret_cast<char*>(output), static_cast<std::streamsize>(size));
    return static_cast<std::size_t>(input.gcount()) == size;
}

std::string fourcc(const unsigned char* data) {
    return std::string(reinterpret_cast<const char*>(data), 4);
}

} // namespace

RiffAnalysis analyze_riff(std::istream& input, std::uint64_t base_offset, std::uint64_t record_size) {
    RiffAnalysis analysis;
    if (record_size < 12) return analysis;
    std::array<unsigned char, 12> header{};
    if (!read_at(input, base_offset, header.data(), header.size())) return analysis;
    if (fourcc(header.data()) != "RIFF") return analysis;
    analysis.declared_file_size = static_cast<std::uint64_t>(read_u32_le(header.data() + 4)) + 8;
    analysis.form_type = fourcc(header.data() + 8);
    analysis.size_matches_record = analysis.declared_file_size == record_size;
    if (analysis.declared_file_size > record_size || analysis.declared_file_size < 12) return analysis;

    std::uint64_t position = 12;
    while (position < analysis.declared_file_size) {
        if (analysis.declared_file_size - position < 8 ||
            base_offset > std::numeric_limits<std::uint64_t>::max() - position) {
            return analysis;
        }
        std::array<unsigned char, 8> chunk_header{};
        if (!read_at(input, base_offset + position, chunk_header.data(), chunk_header.size())) return analysis;
        const auto chunk_size = static_cast<std::uint64_t>(read_u32_le(chunk_header.data() + 4));
        const auto padded_size = chunk_size + (chunk_size & 1U);
        if (padded_size > analysis.declared_file_size - position - 8) return analysis;
        ++analysis.chunk_count;
        ++analysis.chunk_ids[fourcc(chunk_header.data())];
        position += 8 + padded_size;
    }
    analysis.structure_valid = position == analysis.declared_file_size;
    analysis.trailing_bytes_size = record_size - analysis.declared_file_size;
    analysis.trailing_bytes_zero = true;
    std::array<unsigned char, 4096> trailing{};
    std::uint64_t checked = 0;
    while (checked < analysis.trailing_bytes_size) {
        const auto chunk = static_cast<std::size_t>(std::min<std::uint64_t>(
            trailing.size(), analysis.trailing_bytes_size - checked));
        if (!read_at(input, base_offset + analysis.declared_file_size + checked, trailing.data(), chunk)) {
            analysis.trailing_bytes_zero = false;
            break;
        }
        if (!std::all_of(trailing.begin(), trailing.begin() + chunk,
                         [](unsigned char value) { return value == 0; })) {
            analysis.trailing_bytes_zero = false;
            break;
        }
        checked += chunk;
    }
    return analysis;
}

} // namespace rb4pc::inspector
