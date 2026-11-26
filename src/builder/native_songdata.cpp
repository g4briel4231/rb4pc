#include "rb4pc/builder/native_songdata.hpp"
#include "rb4pc/inspector/sha256.hpp"

#include <bit>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace rb4pc::builder {
namespace {
constexpr std::size_t native_songdata_size = 1202;
std::string escape_json(std::string_view value) {
    std::ostringstream output;
    for (const unsigned char c : value) {
        if (c == '"') output << "\\\""; else if (c == '\\') output << "\\\\";
        else if (c == '\n') output << "\\n"; else if (c == '\r') output << "\\r";
        else if (c == '\t') output << "\\t"; else if (c < 32) throw std::runtime_error("native songdata text contains a control character");
        else output << static_cast<char>(c);
    }
    return output.str();
}
void append_u16(std::vector<unsigned char>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<unsigned char>(value)); bytes.push_back(static_cast<unsigned char>(value >> 8U));
}
void append_u32(std::vector<unsigned char>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<unsigned char>(value)); bytes.push_back(static_cast<unsigned char>(value >> 8U));
    bytes.push_back(static_cast<unsigned char>(value >> 16U)); bytes.push_back(static_cast<unsigned char>(value >> 24U));
}
void append_float(std::vector<unsigned char>& bytes, float value) { append_u32(bytes, std::bit_cast<std::uint32_t>(value)); }
void append_fixed_string(std::vector<unsigned char>& bytes, std::string_view value, std::size_t width, const char* field) {
    if (value.size() >= width) throw std::runtime_error(std::string(field) + " is too long for native songdata");
    bytes.insert(bytes.end(), value.begin(), value.end()); bytes.resize(bytes.size() + width - value.size(), 0);
}
std::uint16_t read_u16(const std::vector<unsigned char>& bytes, std::size_t& offset) {
    if (offset + 2 > bytes.size()) throw std::runtime_error("truncated native songdata");
    const auto value = static_cast<std::uint16_t>(bytes[offset]) | static_cast<std::uint16_t>(bytes[offset + 1] << 8U); offset += 2; return value;
}
std::uint32_t read_u32(const std::vector<unsigned char>& bytes, std::size_t& offset) {
    if (offset + 4 > bytes.size()) throw std::runtime_error("truncated native songdata");
    const auto value = static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
                       (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
    offset += 4; return value;
}
float read_float(const std::vector<unsigned char>& bytes, std::size_t& offset) { return std::bit_cast<float>(read_u32(bytes, offset)); }
std::string read_fixed_string(const std::vector<unsigned char>& bytes, std::size_t& offset, std::size_t width) {
    if (offset + width > bytes.size()) throw std::runtime_error("truncated native songdata string");
    std::size_t length = 0; while (length < width && bytes[offset + length] != 0) ++length;
    for (std::size_t index = length; index < width; ++index) if (bytes[offset + index] != 0) throw std::runtime_error("native songdata string has data after terminator");
    std::string value(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.begin() + static_cast<std::ptrdiff_t>(offset + length)); offset += width; return value;
}
bool read_bool(const std::vector<unsigned char>& bytes, std::size_t& offset) {
    if (offset >= bytes.size() || bytes[offset] > 1) throw std::runtime_error("invalid native songdata boolean"); return bytes[offset++] != 0;
}
void expect_zeroes(const std::vector<unsigned char>& bytes, std::size_t& offset, std::size_t count) {
    if (offset + count > bytes.size()) throw std::runtime_error("truncated native songdata padding");
    for (std::size_t index = 0; index < count; ++index) if (bytes[offset++] != 0) throw std::runtime_error("unsupported native songdata padding");
}
} // namespace

NativeSongData read_native_songdata(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary); if (!input) throw std::runtime_error("could not open native songdata");
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (bytes.size() != native_songdata_size) throw std::runtime_error("unsupported native songdata size");
    std::size_t offset = 0; NativeSongData data;
    data.type = read_u32(bytes, offset); data.song_id = read_u32(bytes, offset); data.version = static_cast<std::int16_t>(read_u16(bytes, offset));
    data.game_origin = read_fixed_string(bytes, offset, 18); data.preview_start = read_float(bytes, offset); data.preview_end = read_float(bytes, offset);
    data.name = read_fixed_string(bytes, offset, 256); data.artist = read_fixed_string(bytes, offset, 256); data.album_name = read_fixed_string(bytes, offset, 256);
    data.album_track_number = static_cast<std::int16_t>(read_u16(bytes, offset)); expect_zeroes(bytes, offset, 2);
    data.album_year = static_cast<std::int32_t>(read_u32(bytes, offset)); data.original_year = static_cast<std::int32_t>(read_u32(bytes, offset));
    data.genre = read_fixed_string(bytes, offset, 64); data.song_length = read_float(bytes, offset);
    data.guitar_rank = read_float(bytes, offset); data.bass_rank = read_float(bytes, offset); data.vocals_rank = read_float(bytes, offset);
    data.drum_rank = read_float(bytes, offset); data.band_rank = read_float(bytes, offset); data.keys_rank = read_float(bytes, offset); data.real_keys_rank = read_float(bytes, offset);
    data.tutorial = read_bool(bytes, offset); data.album_art = read_bool(bytes, offset); data.cover = read_bool(bytes, offset);
    if (offset >= bytes.size()) throw std::runtime_error("truncated native songdata vocal gender"); data.vocal_gender = bytes[offset++];
    data.medium = read_fixed_string(bytes, offset, 16); data.has_freestyle_vocals = read_bool(bytes, offset); expect_zeroes(bytes, offset, 3);
    data.vocal_parts = static_cast<std::int32_t>(read_u32(bytes, offset)); data.flags = static_cast<std::int32_t>(read_u32(bytes, offset)); data.fake = read_bool(bytes, offset);
    data.short_name = read_fixed_string(bytes, offset, 257); if (offset != bytes.size()) throw std::runtime_error("unexpected native songdata trailing bytes"); return data;
}

NativeSongDataWriteResult write_native_songdata(const NativeSongData& data, const std::filesystem::path& destination) {
    if (destination.empty() || std::filesystem::exists(destination)) throw std::runtime_error("native songdata destination is empty or already exists");
    const auto parent = destination.has_parent_path() ? destination.parent_path() : std::filesystem::path{"."};
    if (!std::filesystem::is_directory(parent)) throw std::runtime_error("native songdata destination parent does not exist");
    auto temporary = destination; temporary += ".rb4pc-part"; auto manifest = destination; manifest += ".rb4pc-conversion.json";
    auto manifest_temporary = manifest; manifest_temporary += ".rb4pc-part";
    if (std::filesystem::exists(temporary) || std::filesystem::exists(manifest) || std::filesystem::exists(manifest_temporary)) throw std::runtime_error("native songdata temporary or manifest path already exists");
    std::vector<unsigned char> bytes; bytes.reserve(native_songdata_size);
    append_u32(bytes, data.type); append_u32(bytes, data.song_id); append_u16(bytes, static_cast<std::uint16_t>(data.version));
    append_fixed_string(bytes, data.game_origin, 18, "gameOrigin"); append_float(bytes, data.preview_start); append_float(bytes, data.preview_end);
    append_fixed_string(bytes, data.name, 256, "name"); append_fixed_string(bytes, data.artist, 256, "artist"); append_fixed_string(bytes, data.album_name, 256, "albumName");
    append_u16(bytes, static_cast<std::uint16_t>(data.album_track_number)); bytes.insert(bytes.end(), 2, 0); append_u32(bytes, static_cast<std::uint32_t>(data.album_year));
    append_u32(bytes, static_cast<std::uint32_t>(data.original_year)); append_fixed_string(bytes, data.genre, 64, "genre"); append_float(bytes, data.song_length);
    append_float(bytes, data.guitar_rank); append_float(bytes, data.bass_rank); append_float(bytes, data.vocals_rank); append_float(bytes, data.drum_rank);
    append_float(bytes, data.band_rank); append_float(bytes, data.keys_rank); append_float(bytes, data.real_keys_rank);
    bytes.push_back(data.tutorial); bytes.push_back(data.album_art); bytes.push_back(data.cover); bytes.push_back(data.vocal_gender);
    append_fixed_string(bytes, data.medium, 16, "medium"); bytes.push_back(data.has_freestyle_vocals); bytes.insert(bytes.end(), 3, 0);
    append_u32(bytes, static_cast<std::uint32_t>(data.vocal_parts)); append_u32(bytes, static_cast<std::uint32_t>(data.flags)); bytes.push_back(data.fake);
    append_fixed_string(bytes, data.short_name, 257, "shortName");
    if (bytes.size() != native_songdata_size) throw std::runtime_error("internal native songdata size mismatch");
    NativeSongDataWriteResult result{destination, manifest, data, {}};
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc); output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); output.close();
        if (!output) throw std::runtime_error("could not write native songdata");
        if (read_native_songdata(temporary) != data) throw std::runtime_error("native songdata round-trip verification failed");
        result.destination_sha256 = inspector::sha256_file(temporary);
        std::ofstream manifest_output(manifest_temporary, std::ios::binary | std::ios::trunc);
        manifest_output << "{\n  \"schemaVersion\": 1,\n  \"operation\": \"native-songdta-pc-write\",\n  \"destinationSha256\": \"" << result.destination_sha256 << "\"\n}\n";
        manifest_output.close(); if (!manifest_output) throw std::runtime_error("could not write native songdata manifest");
        std::filesystem::rename(temporary, destination); std::filesystem::rename(manifest_temporary, manifest);
    } catch (...) {
        std::error_code ignored; std::filesystem::remove(temporary, ignored); std::filesystem::remove(manifest_temporary, ignored);
        std::filesystem::remove(destination, ignored); std::filesystem::remove(manifest, ignored); throw;
    }
    return result;
}

NativeSongDataWriteResult roundtrip_native_songdata(const std::filesystem::path& source, const std::filesystem::path& destination) {
    const auto source_hash = inspector::sha256_file(source); auto result = write_native_songdata(read_native_songdata(source), destination);
    if (result.destination_sha256 != source_hash) { std::error_code ignored; std::filesystem::remove(result.destination, ignored); std::filesystem::remove(result.manifest, ignored); throw std::runtime_error("native songdata byte-exact round-trip failed"); }
    return result;
}

std::string format_json(const NativeSongData& data) {
    std::ostringstream output; output << std::setprecision(std::numeric_limits<float>::max_digits10)
        << "{\n  \"schemaVersion\": 1,\n  \"type\": " << data.type << ",\n  \"songId\": " << data.song_id
        << ",\n  \"version\": " << data.version << ",\n  \"gameOrigin\": \"" << escape_json(data.game_origin)
        << "\",\n  \"name\": \"" << escape_json(data.name) << "\",\n  \"artist\": \"" << escape_json(data.artist)
        << "\",\n  \"albumName\": \"" << escape_json(data.album_name) << "\",\n  \"genre\": \"" << escape_json(data.genre)
        << "\",\n  \"shortName\": \"" << escape_json(data.short_name) << "\",\n  \"previewStart\": " << data.preview_start
        << ",\n  \"previewEnd\": " << data.preview_end << ",\n  \"songLength\": " << data.song_length << "\n}\n"; return output.str();
}
} // namespace rb4pc::builder