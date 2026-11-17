#include "rb4pc/runtime/audio_output.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 3) { std::cerr << "Usage: " << argv[0] << " <header.hdr> <wave-path>\n"; return 2; }
    try {
        const auto vfs = rb4pc::runtime::ArchiveFileSystem::mount(argv[1]);
        const auto wave = rb4pc::runtime::load_wave(vfs, argv[2]);
        std::cout << "Playing " << wave.path << " (" << wave.duration_seconds << " seconds)\n";
        rb4pc::runtime::play_wave_blocking(vfs, wave);
        std::cout << "Playback complete\n";
    } catch (const std::exception& error) { std::cerr << "rb4pc-audio-play: " << error.what() << '\n'; return 1; }
}
