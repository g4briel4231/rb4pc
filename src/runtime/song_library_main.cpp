#include "rb4pc/runtime/song_library.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <songs-directory>\n";
        return 2;
    }
    try {
        const auto library = rb4pc::runtime::scan_song_library(argv[1]);
        std::cout << "Song library: " << library.root.string()
                  << "\nValid songs: " << library.valid_entries << '/' << library.entries.size() << '\n';
        for (const auto& entry : library.entries) {
            std::cout << "  " << entry.short_name << ": " << (entry.valid ? "valid" : "invalid");
            if (!entry.error.empty()) std::cout << " (" << entry.error << ')';
            std::cout << '\n';
        }
        return library.valid_entries == library.entries.size() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "rb4pc-song-library: " << error.what() << '\n';
        return 1;
    }
}
