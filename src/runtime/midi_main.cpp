#include "rb4pc/runtime/midi_parser.hpp"
#include <iostream>
int main(int argc, char* argv[]) {
    if (argc != 2) { std::cerr << "Usage: " << argv[0] << " <notes.mid>\n"; return 2; }
    try { const auto midi = rb4pc::runtime::parse_standard_midi(argv[1]);
        std::cout << "MIDI size: " << midi.header.file_size << " bytes\nFormat: " << midi.header.format << "\nTracks: " << midi.header.track_count << "\nDivision: " << midi.header.division << "\nTempo events: " << midi.tempo_events << '\n';
        for (std::size_t i = 0; i < midi.tracks.size(); ++i) { const auto& track = midi.tracks[i];
            std::cout << "  [" << i << "] " << (track.name.empty() ? "<unnamed>" : track.name) << ": events=" << track.event_count << ", noteOn=" << track.note_on_count << ", endTick=" << track.end_tick << '\n'; }
    } catch (const std::exception& e) { std::cerr << "rb4pc-midi-probe: " << e.what() << '\n'; return 1; }
}
