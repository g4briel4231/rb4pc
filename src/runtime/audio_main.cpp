#include "rb4pc/runtime/wave_loader.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 3) { std::cerr << "Usage: " << argv[0] << " <header.hdr> <wave-path>\n"; return 2; }
    try {
        const auto vfs = rb4pc::runtime::ArchiveFileSystem::mount(argv[1]);
        const auto wave = rb4pc::runtime::load_wave(vfs, argv[2]);
        std::cout << "PCM format: " << wave.format_tag << '\n' << "Channels: " << wave.channels << '\n'
                  << "Sample rate: " << wave.sample_rate << " Hz\n" << "Bits per sample: " << wave.bits_per_sample << '\n'
                  << "Frames: " << wave.frame_count << '\n' << "Duration: " << wave.duration_seconds << " seconds\n";
    } catch (const std::exception& error) { std::cerr << "rb4pc-audio-probe: " << error.what() << '\n'; return 1; }
}
