#include "rb4pc/builder/song_metadata_writer.hpp"
#include "rb4pc/inspector/sha256.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace rb4pc::builder {
namespace {
std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
std::string escape_json(std::string_view value) {
    std::ostringstream output;
    for (const unsigned char c : value) {
        if (c == '"') output << "\\\"";
        else if (c == '\\') output << "\\\\";
        else if (c == '\n') output << "\\n";
        else if (c == '\r') output << "\\r";
        else if (c == '\t') output << "\\t";
        else if (c < 32) throw std::runtime_error("metadata text contains an unsupported control character");
        else output << static_cast<char>(c);
    }
    return output.str();
}
std::string unescape_json(std::string_view value) {
    std::string output;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\') { output += value[i]; continue; }
        if (++i >= value.size()) throw std::runtime_error("invalid JSON escape");
        if (value[i] == 'n') output += '\n';
        else if (value[i] == 'r') output += '\r';
        else if (value[i] == 't') output += '\t';
        else if (value[i] == '"' || value[i] == '\\') output += value[i];
        else throw std::runtime_error("unsupported JSON escape");
    }
    return output;
}
std::string json_string(const std::string& text, const char* key) {
    const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"])*)\\\"");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) throw std::runtime_error(std::string("canonical metadata is missing ") + key);
    return unescape_json(match[1].str());
}
std::optional<std::int64_t> json_integer(const std::string& text, const char* key) {
    const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*(null|-?[0-9]+)");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) throw std::runtime_error(std::string("canonical metadata is missing ") + key);
    if (match[1].str() == "null") return std::nullopt;
    return std::stoll(match[1].str());
}
std::string derive_short_name(const std::filesystem::path& path) {
    auto value = lower(path.parent_path().filename().string());
    if (value.empty()) value = lower(path.stem().string());
    for (char& c : value) if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') c = '_';
    if (value.empty()) throw std::runtime_error("could not derive a song short name");
    return value;
}
} // namespace

CanonicalSongMetadata parse_song_ini(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("could not open song.ini");
    std::unordered_map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue;
        const auto equals = line.find('=');
        if (equals == std::string::npos) continue;
        values[lower(trim(line.substr(0, equals)))] = trim(line.substr(equals + 1));
    }
    CanonicalSongMetadata metadata;
    metadata.short_name = values.contains("shortname") ? values.at("shortname") : derive_short_name(path);
    if (values.contains("name")) metadata.title = values.at("name");
    else if (values.contains("title")) metadata.title = values.at("title");
    if (values.contains("artist")) metadata.artist = values.at("artist");
    if (values.contains("album")) metadata.album = values.at("album");
    if (values.contains("genre")) metadata.genre = values.at("genre");
    if (metadata.title.empty() || metadata.artist.empty()) throw std::runtime_error("song.ini requires name/title and artist");
    if (values.contains("year") && !values.at("year").empty()) {
        const auto year = std::stoll(values.at("year"));
        if (year < std::numeric_limits<std::int32_t>::min() || year > std::numeric_limits<std::int32_t>::max()) throw std::runtime_error("year is outside int32 range");
        metadata.year = static_cast<std::int32_t>(year);
    }
    if (values.contains("preview_start_time") && !values.at("preview_start_time").empty()) {
        const auto preview = std::stoll(values.at("preview_start_time"));
        if (preview < 0 || preview > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("preview_start_time is outside uint32 range");
        metadata.preview_start_ms = static_cast<std::uint32_t>(preview);
    }
    return metadata;
}

std::string format_json(const CanonicalSongMetadata& metadata) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n"
           << "  \"shortName\": \"" << escape_json(metadata.short_name) << "\",\n"
           << "  \"title\": \"" << escape_json(metadata.title) << "\",\n"
           << "  \"artist\": \"" << escape_json(metadata.artist) << "\",\n"
           << "  \"album\": \"" << escape_json(metadata.album) << "\",\n"
           << "  \"genre\": \"" << escape_json(metadata.genre) << "\",\n  \"year\": ";
    if (metadata.year) output << *metadata.year; else output << "null";
    output << ",\n  \"previewStartMs\": ";
    if (metadata.preview_start_ms) output << *metadata.preview_start_ms; else output << "null";
    output << "\n}\n";
    return output.str();
}

CanonicalSongMetadata read_canonical_song_metadata(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open canonical metadata");
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (text.find("\"schemaVersion\": 1") == std::string::npos) throw std::runtime_error("unsupported canonical metadata schema");
    CanonicalSongMetadata metadata;
    metadata.short_name = json_string(text, "shortName");
    metadata.title = json_string(text, "title");
    metadata.artist = json_string(text, "artist");
    metadata.album = json_string(text, "album");
    metadata.genre = json_string(text, "genre");
    const auto year = json_integer(text, "year");
    if (year) {
        if (*year < std::numeric_limits<std::int32_t>::min() || *year > std::numeric_limits<std::int32_t>::max()) throw std::runtime_error("year is outside int32 range");
        metadata.year = static_cast<std::int32_t>(*year);
    }
    const auto preview = json_integer(text, "previewStartMs");
    if (preview) {
        if (*preview < 0 || *preview > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("previewStartMs is outside uint32 range");
        metadata.preview_start_ms = static_cast<std::uint32_t>(*preview);
    }
    return metadata;
}

SongMetadataConversionResult convert_song_ini_metadata(const std::filesystem::path& source, const std::filesystem::path& destination) {
    if (destination.empty() || std::filesystem::exists(destination)) throw std::runtime_error("metadata destination is empty or already exists");
    const auto parent = destination.has_parent_path() ? destination.parent_path() : std::filesystem::path{"."};
    if (!std::filesystem::is_directory(parent)) throw std::runtime_error("metadata destination parent does not exist");
    auto temporary = destination; temporary += ".rb4pc-part";
    auto manifest = destination; manifest += ".rb4pc-conversion.json";
    auto manifest_temporary = manifest; manifest_temporary += ".rb4pc-part";
    if (std::filesystem::exists(temporary) || std::filesystem::exists(manifest) || std::filesystem::exists(manifest_temporary)) throw std::runtime_error("metadata temporary or manifest path already exists");
    SongMetadataConversionResult result;
    result.destination = destination;
    result.manifest = manifest;
    result.metadata = parse_song_ini(source);
    result.source_sha256 = inspector::sha256_file(source);
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << format_json(result.metadata); output.close();
        if (!output) throw std::runtime_error("could not write canonical metadata");
        if (read_canonical_song_metadata(temporary) != result.metadata) throw std::runtime_error("canonical metadata round-trip verification failed");
        result.destination_sha256 = inspector::sha256_file(temporary);
        std::ofstream manifest_output(manifest_temporary, std::ios::binary | std::ios::trunc);
        manifest_output << "{\n  \"schemaVersion\": 1,\n  \"operation\": \"song-ini-to-rb4pc-metadata\",\n"
                        << "  \"sourceSha256\": \"" << result.source_sha256 << "\",\n"
                        << "  \"destinationSha256\": \"" << result.destination_sha256 << "\"\n}\n";
        manifest_output.close();
        if (!manifest_output) throw std::runtime_error("could not write metadata manifest");
        std::filesystem::rename(temporary, destination);
        std::filesystem::rename(manifest_temporary, manifest);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        std::filesystem::remove(manifest_temporary, ignored);
        std::filesystem::remove(destination, ignored);
        std::filesystem::remove(manifest, ignored);
        throw;
    }
    return result;
}
} // namespace rb4pc::builder