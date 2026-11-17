#include "rb4pc/runtime/runtime_session.hpp"
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 5) { std::cerr << "Usage: " << argv[0] << " <header.hdr> [audio-prefix] [--diagnostics <file>]\n"; return 2; }
    try {
        std::string prefix;
        std::filesystem::path diagnostics;
        for (int index = 2; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--diagnostics" && index + 1 < argc && diagnostics.empty()) diagnostics = argv[++index];
            else if (prefix.empty()) prefix = argument;
            else throw std::runtime_error("invalid runtime arguments");
        }
        const auto session = rb4pc::runtime::RuntimeSession::boot(argv[1]);
        const auto waves = session.discover_wave_assets(prefix, 5);
        std::cout << "Runtime source ready: " << (session.plan().ready ? "yes" : "no") << '\n'
                  << "Mounted records: " << session.vfs().file_count() << '\n'
                  << "Convertible RIFF records: " << session.plan().convertible_riff_records << '\n'
                  << "Discovered PCM WAVE samples: " << waves.size() << '\n';
        for (const auto& wave : waves) std::cout << "  " << wave.path << " — " << wave.channels << "ch " << wave.sample_rate << "Hz " << wave.duration_seconds << "s\n";
        if (!diagnostics.empty()) {
            if (std::filesystem::exists(diagnostics)) throw std::runtime_error("diagnostics destination already exists");
            std::ofstream output(diagnostics, std::ios::binary);
            output << session.diagnostics_json();
            if (!output) throw std::runtime_error("could not write diagnostics");
            std::cout << "Diagnostics: " << diagnostics.string() << '\n';
        }
        return session.plan().ready ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << "rb4pc-runtime: " << error.what() << '\n'; return 1; }
}
