#include "rb4pc/runtime/song_session_verifier.hpp"
#include "rb4pc/runtime/song_directory.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <song-directory>\n"
                  << "   or: " << argv[0] << " <notes.mid> <song.mogg> <song.mogg.dta>\n";
        return 2;
    }
    try {
        const auto files = argc == 2 ? rb4pc::runtime::resolve_song_directory(argv[1])
                                     : rb4pc::runtime::SongDirectoryFiles{{}, argv[1], argv[2], argv[3]};
        const auto result = rb4pc::runtime::verify_song_session(
            files.midi, files.mogg, files.mix_metadata);
        std::cout << "Track: " << result.track_name
                  << "\nAudio frames: " << result.audio_frames
                  << "\nAudio seconds: " << result.audio_seconds
                  << "\nDecoded blocks: " << result.decoded_blocks
                  << "\nChart chords: " << result.chart_chords
                  << "\nAutoplay hits: " << result.autoplay_hits
                  << "\nAutoplay misses: " << result.autoplay_misses
                  << "\nScore: " << result.score
                  << "\nStereo peak: " << result.stereo_peak
                  << "\nClipped samples: " << result.clipped_samples
                  << "\nFinite PCM: " << (result.finite_pcm ? "yes" : "no")
                  << "\nTransport finished: " << (result.transport_finished ? "yes" : "no")
                  << "\nGameplay finished: " << (result.gameplay_finished ? "yes" : "no")
                  << "\nVerification: " << (result.passed ? "passed" : "failed") << '\n';
        return result.passed ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "rb4pc-song-verify: " << error.what() << '\n';
        return 1;
    }
}
