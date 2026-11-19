#include "rb4pc/runtime/song_directory.hpp"

#include <stdexcept>

namespace rb4pc::runtime {

SongDirectoryFiles resolve_song_directory(const std::filesystem::path& directory) {
    if (!std::filesystem::is_directory(directory)) throw std::runtime_error("song path is not a directory");
    const auto normalized = std::filesystem::weakly_canonical(directory);
    const auto short_name = normalized.filename().string();
    if (short_name.empty()) throw std::runtime_error("song directory has no short name");
    SongDirectoryFiles result{normalized, normalized / "notes.mid",
                              normalized / (short_name + ".mogg"),
                              normalized / (short_name + ".mogg.dta")};
    if (!std::filesystem::is_regular_file(result.midi) ||
        !std::filesystem::is_regular_file(result.mogg) ||
        !std::filesystem::is_regular_file(result.mix_metadata))
        throw std::runtime_error("song directory is missing notes.mid, <shortname>.mogg, or <shortname>.mogg.dta");
    return result;
}

} // namespace rb4pc::runtime
