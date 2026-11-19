#include "rb4pc/runtime/mogg_parser.hpp"
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace rb4pc::runtime {
namespace {
std::uint32_t le32(const unsigned char* b) { return static_cast<std::uint32_t>(b[0]) | (static_cast<std::uint32_t>(b[1]) << 8) | (static_cast<std::uint32_t>(b[2]) << 16) | (static_cast<std::uint32_t>(b[3]) << 24); }
std::uint64_t le64(const unsigned char* b) {
    std::uint64_t value = 0;
    for (int index = 7; index >= 0; --index) value = (value << 8) | b[index];
    return value;
}

void read_exact(std::ifstream& input, void* destination, std::size_t size, const char* message) {
    input.read(static_cast<char*>(destination), static_cast<std::streamsize>(size));
    if (input.gcount() != static_cast<std::streamsize>(size)) throw std::runtime_error(message);
}
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

OggVorbisInfo inspect_ogg_vorbis(const std::filesystem::path& path, std::uintmax_t offset) {
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Ogg path is not a file");
    const auto file_size = std::filesystem::file_size(path);
    if (offset > file_size || file_size - offset < 27) throw std::runtime_error("truncated Ogg stream");

    std::ifstream input(path, std::ios::binary);
    input.seekg(static_cast<std::streamoff>(offset));
    OggVorbisInfo result;
    std::vector<unsigned char> packet;
    bool found_identification = false;

    while (static_cast<std::uintmax_t>(input.tellg()) < file_size) {
        std::array<unsigned char, 27> header{};
        read_exact(input, header.data(), header.size(), "truncated Ogg page header");
        if (!std::equal(header.begin(), header.begin() + 4,
                        std::array<unsigned char, 4>{'O', 'g', 'g', 'S'}.begin()))
            throw std::runtime_error("invalid Ogg page signature");
        if (header[4] != 0) throw std::runtime_error("unsupported Ogg bitstream version");

        const auto granule = le64(header.data() + 6);
        if (granule != std::numeric_limits<std::uint64_t>::max()) result.final_granule = granule;
        std::vector<unsigned char> lacing(header[26]);
        read_exact(input, lacing.data(), lacing.size(), "truncated Ogg segment table");
        for (const auto segment_size : lacing) {
            const auto previous_size = packet.size();
            packet.resize(previous_size + segment_size);
            read_exact(input, packet.data() + previous_size, segment_size, "truncated Ogg packet");
            if (segment_size < 255) {
                if (!found_identification) {
                    if (packet.size() < 30 || packet[0] != 1 ||
                        !std::equal(packet.begin() + 1, packet.begin() + 7,
                                    std::array<unsigned char, 6>{'v', 'o', 'r', 'b', 'i', 's'}.begin()))
                        throw std::runtime_error("first Ogg packet is not Vorbis identification");
                    if (le32(packet.data() + 7) != 0) throw std::runtime_error("unsupported Vorbis version");
                    result.channels = packet[11];
                    result.sample_rate = le32(packet.data() + 12);
                    if (result.channels == 0 || result.sample_rate == 0)
                        throw std::runtime_error("invalid Vorbis channel or sample-rate value");
                    found_identification = true;
                }
                packet.clear();
            }
        }
        ++result.page_count;
    }
    if (!found_identification) throw std::runtime_error("Vorbis identification packet not found");
    result.duration_seconds = static_cast<double>(result.final_granule) / result.sample_rate;
    return result;
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
