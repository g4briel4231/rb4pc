#include "rb4pc/builder/native_songdata.hpp"
#include "rb4pc/inspector/sha256.hpp"
#include "rb4pc/builder/song_metadata_writer.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <unordered_map>

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
std::string trim(std::string value) { while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin()); while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back(); return value; }
std::string lower(std::string value) { std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); }); return value; }
std::unordered_map<std::string, std::string> read_ini_values(const std::filesystem::path& path) {
    std::ifstream input(path); if (!input) throw std::runtime_error("could not open song.ini"); std::unordered_map<std::string, std::string> values; std::string line;
    while (std::getline(input, line)) { line = trim(line); if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue; const auto equals = line.find('='); if (equals != std::string::npos) values[lower(trim(line.substr(0, equals)))] = trim(line.substr(equals + 1)); } return values;
}
template <typename T> T required_integer(const std::unordered_map<std::string, std::string>& values, const char* key) {
    const auto found = values.find(key); if (found == values.end() || found->second.empty()) throw std::runtime_error(std::string("song.ini requires ") + key + " for native songdata"); std::size_t used = 0; const auto parsed = std::stoll(found->second, &used);
    if (used != found->second.size() || parsed < static_cast<long long>(std::numeric_limits<T>::min()) || static_cast<unsigned long long>(parsed) > static_cast<unsigned long long>(std::numeric_limits<T>::max())) throw std::runtime_error(std::string(key) + " is outside its native range"); return static_cast<T>(parsed);
}
template <typename T> T optional_integer(const std::unordered_map<std::string, std::string>& values, const char* key, T fallback, std::vector<std::string>& defaults) {
    const auto found = values.find(key); if (found == values.end() || found->second.empty()) { defaults.emplace_back(key); return fallback; } std::size_t used = 0; const auto parsed = std::stoll(found->second, &used);
    if (used != found->second.size() || parsed < static_cast<long long>(std::numeric_limits<T>::min()) || static_cast<unsigned long long>(parsed) > static_cast<unsigned long long>(std::numeric_limits<T>::max())) throw std::runtime_error(std::string(key) + " is outside its native range"); return static_cast<T>(parsed);
}
float required_float(const std::unordered_map<std::string, std::string>& values, const char* key) { const auto found = values.find(key); if (found == values.end() || found->second.empty()) throw std::runtime_error(std::string("song.ini requires ") + key + " for native songdata"); std::size_t used = 0; const auto parsed = std::stof(found->second, &used); if (used != found->second.size() || !std::isfinite(parsed) || parsed < 0.0F) throw std::runtime_error(std::string(key) + " must be finite and nonnegative"); return parsed; }
float optional_float(const std::unordered_map<std::string, std::string>& values, const char* key, float fallback, std::vector<std::string>& defaults) { const auto found = values.find(key); if (found == values.end() || found->second.empty()) { defaults.emplace_back(key); return fallback; } std::size_t used = 0; const auto parsed = std::stof(found->second, &used); if (used != found->second.size() || !std::isfinite(parsed) || parsed < 0.0F) throw std::runtime_error(std::string(key) + " must be finite and nonnegative"); return parsed; }
bool optional_bool(const std::unordered_map<std::string, std::string>& values, const char* key, bool fallback, std::vector<std::string>& defaults) { const auto found = values.find(key); if (found == values.end() || found->second.empty()) { defaults.emplace_back(key); return fallback; } const auto value = lower(found->second); if (value == "1" || value == "true" || value == "yes") return true; if (value == "0" || value == "false" || value == "no") return false; throw std::runtime_error(std::string(key) + " must be true/false or 1/0"); }
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

NativeSongDataConversionResult convert_song_ini_to_native_songdata(const std::filesystem::path& source, const std::filesystem::path& destination) {
    const auto canonical = parse_song_ini(source); const auto values = read_ini_values(source); NativeSongDataConversionResult result; auto& data = result.write.metadata;
    data.type = optional_integer<std::uint32_t>(values, "type", 11, result.defaulted_fields); data.song_id = required_integer<std::uint32_t>(values, "song_id"); data.version = optional_integer<std::int16_t>(values, "version", -1, result.defaulted_fields);
    data.game_origin = values.contains("game_origin") ? values.at("game_origin") : "ugc_plus"; if (!values.contains("game_origin")) result.defaulted_fields.emplace_back("game_origin");
    data.name = canonical.title; data.artist = canonical.artist; data.album_name = canonical.album; data.genre = canonical.genre; data.short_name = canonical.short_name; data.song_length = required_float(values, "song_length");
    data.preview_start = canonical.preview_start_ms ? static_cast<float>(*canonical.preview_start_ms) : 0.0F; if (!canonical.preview_start_ms) result.defaulted_fields.emplace_back("preview_start_time");
    data.preview_end = optional_float(values, "preview_end_time", std::min(data.preview_start + 30000.0F, data.song_length), result.defaulted_fields);
    if (data.preview_start > data.song_length || data.preview_end < data.preview_start || data.preview_end > data.song_length) throw std::runtime_error("native preview range must fit within song_length");
    data.album_track_number = optional_integer<std::int16_t>(values, "album_track_number", 0, result.defaulted_fields); data.album_year = canonical.year.value_or(0); if (!canonical.year) result.defaulted_fields.emplace_back("year");
    data.original_year = optional_integer<std::int32_t>(values, "original_year", data.album_year, result.defaulted_fields);
    data.guitar_rank = optional_float(values, "diff_guitar", 0.0F, result.defaulted_fields); data.bass_rank = optional_float(values, "diff_bass", 0.0F, result.defaulted_fields); data.vocals_rank = optional_float(values, "diff_vocals", 0.0F, result.defaulted_fields);
    data.drum_rank = optional_float(values, "diff_drums", 0.0F, result.defaulted_fields); data.band_rank = optional_float(values, "diff_band", 0.0F, result.defaulted_fields); data.keys_rank = optional_float(values, "diff_keys", 0.0F, result.defaulted_fields); data.real_keys_rank = optional_float(values, "diff_real_keys", 0.0F, result.defaulted_fields);
    data.tutorial = optional_bool(values, "tutorial", false, result.defaulted_fields); data.album_art = optional_bool(values, "album_art", true, result.defaulted_fields); data.cover = optional_bool(values, "cover", false, result.defaulted_fields);
    data.vocal_gender = optional_integer<std::uint8_t>(values, "vocal_gender", 0, result.defaulted_fields); data.medium = values.contains("medium") ? values.at("medium") : ""; if (!values.contains("medium")) result.defaulted_fields.emplace_back("medium");
    data.has_freestyle_vocals = optional_bool(values, "freestyle_vocals", false, result.defaulted_fields); data.vocal_parts = optional_integer<std::int32_t>(values, "vocal_parts", 0, result.defaulted_fields); data.flags = optional_integer<std::int32_t>(values, "flags", 0, result.defaulted_fields); data.fake = optional_bool(values, "fake", false, result.defaulted_fields);
    result.source_sha256 = inspector::sha256_file(source); const auto written = write_native_songdata(data, destination); result.write.destination = written.destination; result.write.manifest = written.manifest; result.write.destination_sha256 = written.destination_sha256;
    auto temporary = result.write.manifest; temporary += ".rb4pc-part"; std::ofstream manifest(temporary, std::ios::binary | std::ios::trunc); manifest << "{\n  \"schemaVersion\": 1,\n  \"operation\": \"song-ini-to-native-songdta-pc\",\n  \"sourceSha256\": \"" << result.source_sha256 << "\",\n  \"destinationSha256\": \"" << result.write.destination_sha256 << "\",\n  \"defaultedFields\": [";
    for (std::size_t index = 0; index < result.defaulted_fields.size(); ++index) { if (index) manifest << ", "; manifest << '"' << escape_json(result.defaulted_fields[index]) << '"'; } manifest << "]\n}\n"; manifest.close(); if (!manifest) throw std::runtime_error("could not write native songdata conversion manifest");
    std::error_code ignored; std::filesystem::remove(result.write.manifest, ignored); std::filesystem::rename(temporary, result.write.manifest); return result;
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