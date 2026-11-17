#include "rb4pc/runtime/midi_parser.hpp"
#include <iostream>
int main(int argc, char* argv[]) {
    if (argc != 2) { std::cerr << "Usage: " << argv[0] << " <notes.mid>\n"; return 2; }
    try { const auto midi = rb4pc::runtime::parse_standard_midi_header(argv[1]);
        std::cout << "MIDI size: " << midi.file_size << " bytes\nFormat: " << midi.format << "\nTracks: " << midi.track_count << "\nDivision: " << midi.division << '\n';
    } catch (const std::exception& e) { std::cerr << "rb4pc-midi-probe: " << e.what() << '\n'; return 1; }
}
