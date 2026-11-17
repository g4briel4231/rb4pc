#include "rb4pc/runtime/runtime_session.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) { std::cerr << "Usage: " << argv[0] << " <header.hdr> [audio-prefix]\n"; return 2; }
    try {
        const auto session = rb4pc::runtime::RuntimeSession::boot(argv[1]);
        const auto prefix = argc == 3 ? argv[2] : "";
        const auto waves = session.discover_wave_assets(prefix, 5);
        std::cout << "Runtime source ready: " << (session.plan().ready ? "yes" : "no") << '\n'
                  << "Mounted records: " << session.vfs().file_count() << '\n'
                  << "Convertible RIFF records: " << session.plan().convertible_riff_records << '\n'
                  << "Discovered PCM WAVE samples: " << waves.size() << '\n';
        for (const auto& wave : waves) std::cout << "  " << wave.path << " — " << wave.channels << "ch " << wave.sample_rate << "Hz " << wave.duration_seconds << "s\n";
        return session.plan().ready ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << "rb4pc-runtime: " << error.what() << '\n'; return 1; }
}
