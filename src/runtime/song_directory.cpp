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
    SongDirectoryFiles result{normalized, normalized / "notes.mid",
                              normalized / (short_name + ".mogg"),
                              normalized / (short_name + ".mogg.dta"),
                              normalized / "session.json", false};
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
    return result;
}

} // namespace rb4pc::runtime
