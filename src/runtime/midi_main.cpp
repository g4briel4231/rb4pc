#include "rb4pc/runtime/midi_parser.hpp"
#include "rb4pc/runtime/five_fret_chart.hpp"
#include "rb4pc/runtime/timed_chart.hpp"
#include "rb4pc/runtime/five_fret_gameplay.hpp"
#include <iostream>
int main(int argc, char* argv[]) {
    if (argc != 2) { std::cerr << "Usage: " << argv[0] << " <notes.mid>\n"; return 2; }
    try { const auto midi = rb4pc::runtime::parse_standard_midi(argv[1]);
        std::cout << "MIDI size: " << midi.header.file_size << " bytes\nFormat: " << midi.header.format << "\nTracks: " << midi.header.track_count << "\nDivision: " << midi.header.division << "\nTempo events: " << midi.tempo_events << '\n';
        std::cout << "Timeline end: " << rb4pc::runtime::midi_tick_to_seconds(midi, midi.tracks.empty() ? 0 : midi.tracks[0].end_tick) << " seconds (track 0)\n";
        for (std::size_t i = 0; i < midi.tracks.size(); ++i) { const auto& track = midi.tracks[i];
            std::cout << "  [" << i << "] " << (track.name.empty() ? "<unnamed>" : track.name) << ": events=" << track.event_count << ", noteOn=" << track.note_on_count << ", paired=" << track.notes.size() << ", unmatchedOff=" << track.unmatched_note_off_count << ", danglingOn=" << track.dangling_note_on_count << ", endTick=" << track.end_tick << '\n'; }
        for (const auto& track : midi.tracks) if (track.name == "PART GUITAR" || track.name == "PART BASS") {
            const auto easy = rb4pc::runtime::build_five_fret_chart(track, rb4pc::runtime::ChartDifficulty::easy);
            const auto medium = rb4pc::runtime::build_five_fret_chart(track, rb4pc::runtime::ChartDifficulty::medium);
            const auto hard = rb4pc::runtime::build_five_fret_chart(track, rb4pc::runtime::ChartDifficulty::hard);
            const auto expert = rb4pc::runtime::build_five_fret_chart(track, rb4pc::runtime::ChartDifficulty::expert);
            std::cout << "  " << track.name << " five-fret chords: easy=" << easy.chords.size() << ", medium=" << medium.chords.size() << ", hard=" << hard.chords.size() << ", expert=" << expert.chords.size() << '\n';
            const auto timed = rb4pc::runtime::time_five_fret_chart(midi, expert);
            if (!timed.chords.empty()) std::cout << "    expert timeline: first=" << timed.chords.front().start_seconds << "s, last=" << timed.chords.back().start_seconds << "s\n";
            rb4pc::runtime::FiveFretGameplay autoplay(timed);
            for (const auto& chord : timed.chords) (void)autoplay.strum(chord.lane_mask, chord.start_seconds);
            std::cout << "    deterministic autoplay: hits=" << autoplay.hit_count() << ", misses=" << autoplay.miss_count() << ", score=" << autoplay.score() << '\n';
        }
    } catch (const std::exception& e) { std::cerr << "rb4pc-midi-probe: " << e.what() << '\n'; return 1; }
}
