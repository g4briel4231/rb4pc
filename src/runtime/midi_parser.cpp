#include "rb4pc/runtime/midi_parser.hpp"
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>

namespace rb4pc::runtime {
MidiHeader parse_standard_midi_header(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("MIDI path is not a file");
    std::ifstream input(path, std::ios::binary); std::array<unsigned char, 14> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
        std::string(reinterpret_cast<const char*>(bytes.data()), 4) != "MThd") throw std::runtime_error("not a standard MIDI file");
    const auto be16 = [&bytes](std::size_t p) { return static_cast<std::uint16_t>((bytes[p] << 8) | bytes[p + 1]); };
    const auto header_size = (static_cast<std::uint32_t>(bytes[4]) << 24) | (static_cast<std::uint32_t>(bytes[5]) << 16) |
                             (static_cast<std::uint32_t>(bytes[6]) << 8) | bytes[7];
    if (header_size != 6) throw std::runtime_error("unsupported MIDI header length");
    return {be16(8), be16(10), be16(12), std::filesystem::file_size(path)};
}
} // namespace rb4pc::runtime
