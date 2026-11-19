#include "rb4pc/runtime/five_fret_import.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <notes.mid> [PART GUITAR|PART BASS]\n";
        return 2;
    }
    try {
        const std::string track = argc == 3 ? argv[2] : "PART GUITAR";
        const auto imported = rb4pc::runtime::import_five_fret_midi(
            argv[1], track, rb4pc::runtime::ChartDifficulty::expert);
        std::cout << "Imported track: " << imported.track_name << "\nExpert chords: "
                  << imported.chart.chords.size() << "\nDuration: "
                  << imported.chart.chords.back().end_seconds << " seconds\n";
    } catch (const std::exception& error) { std::cerr << "rb4pc-session-probe: " << error.what() << '\n'; return 1; }
}
