#include "rb4pc/runtime/interactive_song.hpp"
#include "rb4pc/runtime/song_directory.hpp"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <song-directory>\n"
                  << "   or: " << argv[0] << " <notes.mid> <song.mogg> <song.mogg.dta>\n";
        return 2;
    }
    try {
        const auto files = argc == 2 ? rb4pc::runtime::resolve_song_directory(argv[1])
                                     : rb4pc::runtime::SongDirectoryFiles{{}, argv[1], argv[2], argv[3]};
        const auto result = rb4pc::runtime::run_interactive_song(files);
        std::cout << "Song session ended at " << result.seconds << " seconds\n"
                  << "Hits: " << result.hits << "\nMisses: " << result.misses
                  << "\nScore: " << result.score
                  << "\nCancelled: " << (result.cancelled ? "yes" : "no") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "rb4pc-song-play: " << error.what() << '\n';
        return 1;
    }
}
