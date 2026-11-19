#include "rb4pc/runtime/rbmid_parser.hpp"

#include <array>
#include <fstream>
#include <stdexcept>

namespace rb4pc::runtime {

RBMidHeader parse_rbmid_header(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("RBMID path is not a file");
    std::ifstream input(path, std::ios::binary);
    std::array<unsigned char, 4> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (input.gcount() != static_cast<std::streamsize>(bytes.size())) throw std::runtime_error("truncated RBMID header");
    const auto value = static_cast<std::uint32_t>(bytes[0]) |
                       (static_cast<std::uint32_t>(bytes[1]) << 8) |
                       (static_cast<std::uint32_t>(bytes[2]) << 16) |
                       (static_cast<std::uint32_t>(bytes[3]) << 24);
    if (value != static_cast<std::uint32_t>(RBMidFormat::rb4) &&
        value != static_cast<std::uint32_t>(RBMidFormat::rbvr))
        throw std::runtime_error("unrecognized RBMID format value");
    return {static_cast<RBMidFormat>(value), std::filesystem::file_size(path)};
}

} // namespace rb4pc::runtime
