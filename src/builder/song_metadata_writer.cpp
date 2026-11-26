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
#include <unordered_set>

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
    if (metadata.short_name.empty() || !std::all_of(metadata.short_name.begin(), metadata.short_name.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; }))
        throw std::runtime_error("song short name must contain only letters, digits, or underscores");
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
NativeMoggsongMetadata read_native_moggsong_metadata(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open native moggsong metadata");
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    std::size_t offset = 0;
    auto expect = [&](std::initializer_list<unsigned char> expected) {
        for (const auto value : expected) {
            if (offset >= bytes.size() || bytes[offset++] != value) throw std::runtime_error("unsupported native moggsong structure");
        }
    };
    auto read_u32 = [&]() -> std::uint32_t {
        if (offset + 4 > bytes.size()) throw std::runtime_error("truncated native moggsong metadata");
        const auto value = static_cast<std::uint32_t>(bytes[offset]) |
                           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
                           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
                           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
        offset += 4; return value;
    };
    auto read_string = [&]() -> std::string {
        const auto length = read_u32();
        if (offset + length > bytes.size()) throw std::runtime_error("truncated native moggsong string");
        std::string value(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                          bytes.begin() + static_cast<std::ptrdiff_t>(offset + length));
        offset += length; return value;
    };
    auto read_entry = [&](const char* expected_key) -> std::string {
        expect({0x10,0,0,0,1,0,0,0,2,0,1,0,5,0,0,0});
        if (read_string() != expected_key) throw std::runtime_error("unexpected native moggsong key");
        expect({0x12,0,0,0});
        return read_string();
    };
    expect({1,1,0,0,0,2,0,1,0});
    NativeMoggsongMetadata metadata;
    metadata.mogg_path = read_entry("mogg_path");
    metadata.midi_path = read_entry("midi_path");
    if (offset != bytes.size()) throw std::runtime_error("unexpected trailing native moggsong data");
    return metadata;
}

NativeMoggsongWriteResult write_native_moggsong_metadata(const NativeMoggsongMetadata& metadata,
                                                          const std::filesystem::path& destination) {
    if (metadata.mogg_path.empty() || metadata.midi_path.empty()) throw std::runtime_error("native moggsong paths cannot be empty");
    if (metadata.mogg_path.find_first_of("/\\") != std::string::npos || metadata.midi_path.find_first_of("/\\") != std::string::npos)
        throw std::runtime_error("native moggsong paths must be filenames");
    if (!metadata.mogg_path.ends_with(".mogg") || !metadata.midi_path.ends_with(".rbmid"))
        throw std::runtime_error("native moggsong paths require .mogg and .rbmid extensions");
    if (destination.empty() || std::filesystem::exists(destination)) throw std::runtime_error("native moggsong destination is empty or already exists");
    const auto parent = destination.has_parent_path() ? destination.parent_path() : std::filesystem::path{"."};
    if (!std::filesystem::is_directory(parent)) throw std::runtime_error("native moggsong destination parent does not exist");
    auto temporary = destination; temporary += ".rb4pc-part";
    auto manifest = destination; manifest += ".rb4pc-conversion.json";
    auto manifest_temporary = manifest; manifest_temporary += ".rb4pc-part";
    if (std::filesystem::exists(temporary) || std::filesystem::exists(manifest) || std::filesystem::exists(manifest_temporary))
        throw std::runtime_error("native moggsong temporary or manifest path already exists");
    const auto append_u32 = [](std::vector<unsigned char>& bytes, std::uint32_t value) {
        bytes.push_back(static_cast<unsigned char>(value)); bytes.push_back(static_cast<unsigned char>(value >> 8U));
        bytes.push_back(static_cast<unsigned char>(value >> 16U)); bytes.push_back(static_cast<unsigned char>(value >> 24U));
    };
    const auto append_string = [&](std::vector<unsigned char>& bytes, std::string_view value) {
        if (value.size() > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("native moggsong string is too long");
        append_u32(bytes, static_cast<std::uint32_t>(value.size())); bytes.insert(bytes.end(), value.begin(), value.end());
    };
    std::vector<unsigned char> bytes{1,1,0,0,0,2,0,1,0};
    const auto append_entry = [&](std::string_view key, std::string_view value) {
        const unsigned char prefix[]{0x10,0,0,0,1,0,0,0,2,0,1,0,5,0,0,0};
        bytes.insert(bytes.end(), std::begin(prefix), std::end(prefix)); append_string(bytes, key);
        bytes.insert(bytes.end(), {0x12,0,0,0}); append_string(bytes, value);
    };
    append_entry("mogg_path", metadata.mogg_path); append_entry("midi_path", metadata.midi_path);
    NativeMoggsongWriteResult result{destination, manifest, metadata, {}};
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); output.close();
        if (!output) throw std::runtime_error("could not write native moggsong metadata");
        if (read_native_moggsong_metadata(temporary) != metadata) throw std::runtime_error("native moggsong round-trip verification failed");
        result.destination_sha256 = inspector::sha256_file(temporary);
        std::ofstream manifest_output(manifest_temporary, std::ios::binary | std::ios::trunc);
        manifest_output << "{\n  \"schemaVersion\": 1,\n  \"operation\": \"native-moggsong-dta-pc-write\",\n"
                        << "  \"destinationSha256\": \"" << result.destination_sha256 << "\"\n}\n";
        manifest_output.close(); if (!manifest_output) throw std::runtime_error("could not write native moggsong manifest");
        std::filesystem::rename(temporary, destination); std::filesystem::rename(manifest_temporary, manifest);
    } catch (...) {
        std::error_code ignored; std::filesystem::remove(temporary, ignored); std::filesystem::remove(manifest_temporary, ignored);
        std::filesystem::remove(destination, ignored); std::filesystem::remove(manifest, ignored); throw;
    }
    return result;
}
std::string format_json(const SongMetadataBatchResult& result) {
    std::ostringstream output;
    output << "{\n  \"schemaVersion\": 1,\n  \"operation\": \"song-metadata-batch\",\n"
           << "  \"converted\": " << result.items.size() << ",\n  \"items\": [";
    for (std::size_t index = 0; index < result.items.size(); ++index) {
        const auto& item = result.items[index];
        if (index) output << ',';
        output << "\n    {\"source\": \"" << escape_json(item.source_relative_path.generic_string())
               << "\", \"metadata\": \"" << escape_json(item.metadata_relative_path.generic_string())
               << "\", \"manifest\": \"" << escape_json(item.manifest_relative_path.generic_string())
               << "\", \"shortName\": \"" << escape_json(item.metadata.short_name)
               << "\", \"title\": \"" << escape_json(item.metadata.title)
               << "\", \"artist\": \"" << escape_json(item.metadata.artist)
               << "\", \"sourceSha256\": \"" << item.source_sha256
               << "\", \"destinationSha256\": \"" << item.destination_sha256 << "\"}";
    }
    output << "\n  ]\n}\n";
    return output.str();
}

SongMetadataBatchResult convert_song_metadata_batch(const std::filesystem::path& source_root,
                                                     const std::filesystem::path& destination_root) {
    if (!std::filesystem::is_directory(source_root)) throw std::runtime_error("song metadata source root is not a directory");
    if (destination_root.empty() || std::filesystem::exists(destination_root)) throw std::runtime_error("song metadata batch destination is empty or already exists");
    const auto parent = destination_root.has_parent_path() ? destination_root.parent_path() : std::filesystem::path{"."};
    if (!std::filesystem::is_directory(parent)) throw std::runtime_error("song metadata batch destination parent does not exist");

    struct Candidate { std::filesystem::path source; CanonicalSongMetadata metadata; };
    std::vector<Candidate> candidates;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(source_root)) {
        if (!entry.is_regular_file() || lower(entry.path().filename().string()) != "song.ini") continue;
        candidates.push_back({entry.path(), parse_song_ini(entry.path())});
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
        return left.source.generic_string() < right.source.generic_string();
    });
    if (candidates.empty()) throw std::runtime_error("no song.ini files were found");
    std::unordered_set<std::string> short_names;
    for (const auto& candidate : candidates) {
        const auto normalized = lower(candidate.metadata.short_name);
        if (!short_names.insert(normalized).second) throw std::runtime_error("duplicate song short name in metadata batch: " + candidate.metadata.short_name);
    }

    auto temporary = destination_root; temporary += ".rb4pc-part";
    if (std::filesystem::exists(temporary)) throw std::runtime_error("song metadata batch temporary directory already exists");
    SongMetadataBatchResult result;
    result.directory = destination_root;
    result.summary = destination_root / "batch-summary.json";
    try {
        std::filesystem::create_directory(temporary);
        for (const auto& candidate : candidates) {
            const auto item_directory = temporary / candidate.metadata.short_name;
            std::filesystem::create_directory(item_directory);
            const auto converted = convert_song_ini_metadata(candidate.source, item_directory / "metadata.json");
            SongMetadataBatchItem item;
            item.source_relative_path = std::filesystem::relative(candidate.source, source_root);
            item.metadata_relative_path = std::filesystem::path{candidate.metadata.short_name} / "metadata.json";
            item.manifest_relative_path = std::filesystem::path{candidate.metadata.short_name} / "metadata.json.rb4pc-conversion.json";
            item.metadata = converted.metadata;
            item.source_sha256 = converted.source_sha256;
            item.destination_sha256 = converted.destination_sha256;
            result.items.push_back(std::move(item));
        }
        const auto temporary_summary = temporary / "batch-summary.json";
        std::ofstream output(temporary_summary, std::ios::binary | std::ios::trunc);
        output << format_json(result); output.close();
        if (!output) throw std::runtime_error("could not write song metadata batch summary");
        for (const auto& item : result.items) {
            const auto metadata_path = temporary / item.metadata_relative_path;
            if (read_canonical_song_metadata(metadata_path) != item.metadata) throw std::runtime_error("song metadata batch verification failed");
            if (inspector::sha256_file(metadata_path) != item.destination_sha256) throw std::runtime_error("song metadata batch hash verification failed");
        }
        std::filesystem::rename(temporary, destination_root);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(temporary, ignored);
        std::filesystem::remove_all(destination_root, ignored);
        throw;
    }
    return result;
}
} // namespace rb4pc::builder