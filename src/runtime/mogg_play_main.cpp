#include "rb4pc/runtime/mogg_audio_output.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.mogg> <input.mogg.dta>\n";
        return 2;
    }
    try {
        std::cout << "Playing compatible MOGG through stereo waveOut\n";
        rb4pc::runtime::play_mogg_blocking(argv[1], argv[2]);
        std::cout << "Playback complete\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "rb4pc-mogg-play: " << error.what() << '\n';
        return 1;
    }
}
