#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <map>
#include <string>

namespace rb4pc::inspector {

struct RiffAnalysis {
    std::string form_type;
    std::uint64_t declared_file_size = 0;
    std::size_t chunk_count = 0;
    std::map<std::string, std::size_t> chunk_ids;
    bool size_matches_record = false;
    std::uint64_t trailing_bytes_size = 0;
    bool trailing_bytes_zero = false;
    bool structure_valid = false;
};

[[nodiscard]] RiffAnalysis analyze_riff(
    std::istream& input,
    std::uint64_t base_offset,
    std::uint64_t record_size);

} // namespace rb4pc::inspector
