#include "rb4pc/runtime/mogg_parser.hpp"
#include <array>
#include <fstream>
#include <stdexcept>
namespace rb4pc::runtime {
namespace {
std::uint32_t le32(const unsigned char* b) { return static_cast<std::uint32_t>(b[0]) | (static_cast<std::uint32_t>(b[1]) << 8) | (static_cast<std::uint32_t>(b[2]) << 16) | (static_cast<std::uint32_t>(b[3]) << 24); }
}
MoggHeader parse_mogg_header(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("MOGG path is not a file");
    const auto size = std::filesystem::file_size(path); if (size < 12) throw std::runtime_error("truncated MOGG header");
    std::ifstream input(path, std::ios::binary); std::array<unsigned char, 8> prefix{};
    input.read(reinterpret_cast<char*>(prefix.data()), static_cast<std::streamsize>(prefix.size()));
    const auto version = le32(prefix.data()); const auto offset = le32(prefix.data() + 4);
    if (version != 10) throw std::runtime_error("unsupported MOGG version");
    if (offset < 8 || offset > size - 4) throw std::runtime_error("MOGG Ogg offset is outside the file");
    input.seekg(offset); std::array<char, 4> signature{}; input.read(signature.data(), 4);
    return {version, offset, size, signature == std::array<char, 4>{'O', 'g', 'g', 'S'}};
}
void extract_mogg_ogg(const std::filesystem::path& source, const MoggHeader& header,
                      const std::filesystem::path& destination) {
    if (!header.embedded_ogg) throw std::runtime_error("MOGG does not expose an Ogg stream");
    if (std::filesystem::exists(destination)) throw std::runtime_error("Ogg destination already exists");
    std::ifstream input(source, std::ios::binary); input.seekg(header.ogg_offset);
    std::ofstream output(destination, std::ios::binary); if (!input || !output) throw std::runtime_error("could not open MOGG extraction paths");
    std::array<char, 64 * 1024> buffer{};
    while (input) { input.read(buffer.data(), static_cast<std::streamsize>(buffer.size())); output.write(buffer.data(), input.gcount()); }
    if (!output) throw std::runtime_error("could not write extracted Ogg stream");
}
} // namespace rb4pc::runtime
