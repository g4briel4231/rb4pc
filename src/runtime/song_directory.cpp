#include "rb4pc/runtime/song_directory.hpp"

#include "rb4pc/inspector/sha256.hpp"

#include <fstream>
#include <iterator>
#include <regex>
#include <stdexcept>

namespace rb4pc::runtime {

SongDirectoryFiles resolve_song_directory(const std::filesystem::path& directory) {
    if (!std::filesystem::is_directory(directory)) throw std::runtime_error("song path is not a directory");
    const auto normalized = std::filesystem::weakly_canonical(directory);
    const auto short_name = normalized.filename().string();
    if (short_name.empty()) throw std::runtime_error("song directory has no short name");
    SongDirectoryFiles result;
    result.directory = normalized;
    result.midi = normalized / "notes.mid";
    result.mogg = normalized / (short_name + ".mogg");
    result.mix_metadata = normalized / (short_name + ".mogg.dta");
    result.manifest = normalized / "session.json";
    if (!std::filesystem::is_regular_file(result.midi) ||
        !std::filesystem::is_regular_file(result.mogg) ||
        !std::filesystem::is_regular_file(result.mix_metadata))
        throw std::runtime_error("song directory is missing notes.mid, <shortname>.mogg, or <shortname>.mogg.dta");
    if (!std::filesystem::is_regular_file(result.manifest))
        throw std::runtime_error("song directory is missing session.json");
    std::ifstream manifest_input(result.manifest, std::ios::binary);
    const std::string manifest{std::istreambuf_iterator<char>(manifest_input),
                               std::istreambuf_iterator<char>()};
    const std::regex hashes_pattern(
        R"regex("hashes"\s*:\s*\{\s*"midi"\s*:\s*"([0-9a-f]{64})"\s*,\s*"mogg"\s*:\s*"([0-9a-f]{64})"\s*,\s*"mixMetadata"\s*:\s*"([0-9a-f]{64})"\s*\})regex");
    std::smatch hashes;
    if (!std::regex_search(manifest, hashes, hashes_pattern))
        throw std::runtime_error("song session manifest has no valid hash set");
    if (inspector::sha256_file(result.midi) != hashes[1].str() ||
        inspector::sha256_file(result.mogg) != hashes[2].str() ||
        inspector::sha256_file(result.mix_metadata) != hashes[3].str())
        throw std::runtime_error("song directory content does not match session.json hashes");
    result.integrity_verified = true;
    const std::regex schema_key_pattern(R"regex("schemaVersion"\s*:)regex");
    const std::regex schema_pattern(R"regex("schemaVersion"\s*:\s*([0-9]+))regex");
    std::smatch schema;
    const auto has_schema_key = std::regex_search(manifest, schema_key_pattern);
    const auto has_valid_schema = std::regex_search(manifest, schema, schema_pattern);
    if (has_schema_key && !has_valid_schema)
        throw std::runtime_error("song session manifest has a malformed schema version");
    const auto schema_version = has_valid_schema ? std::stoi(schema[1].str()) : 0;
    if (schema_version != 0 && schema_version != 1 && schema_version != 2)
        throw std::runtime_error("song session manifest uses an unsupported schema version");
    const std::regex artwork_key_pattern(R"regex("artwork"\s*:)regex");
    const auto has_artwork_key = std::regex_search(manifest, artwork_key_pattern);
    const std::regex artwork_pattern(
        R"regex("artwork"\s*:\s*\{\s*"file"\s*:\s*"([^"]+)"\s*,\s*"sha256"\s*:\s*"([0-9a-f]{64})"\s*,\s*"conversionManifest"\s*:\s*"([^"]+)"\s*,\s*"conversionManifestSha256"\s*:\s*"([0-9a-f]{64})"\s*\})regex");
    std::smatch artwork;
    const auto has_valid_artwork = std::regex_search(manifest, artwork, artwork_pattern);
    if (has_artwork_key && !has_valid_artwork)
        throw std::runtime_error("song artwork manifest is malformed");
    if ((schema_version == 2) != has_artwork_key)
        throw std::runtime_error("song artwork does not match the session manifest schema");
    if (has_valid_artwork) {
        const auto expected_artwork_name = short_name + ".png_pc";
        const auto expected_conversion_name = expected_artwork_name + ".rb4pc-conversion.json";
        if (artwork[1].str() != expected_artwork_name || artwork[3].str() != expected_conversion_name)
            throw std::runtime_error("song artwork manifest uses unexpected filenames");
        result.artwork = normalized / expected_artwork_name;
        result.artwork_conversion_manifest = normalized / expected_conversion_name;
        if (!std::filesystem::is_regular_file(*result.artwork) ||
            !std::filesystem::is_regular_file(*result.artwork_conversion_manifest) ||
            inspector::sha256_file(*result.artwork) != artwork[2].str() ||
            inspector::sha256_file(*result.artwork_conversion_manifest) != artwork[4].str())
            throw std::runtime_error("song artwork content does not match session.json hashes");
        result.artwork_integrity_verified = true;
    }
    return result;
}

} // namespace rb4pc::runtime
