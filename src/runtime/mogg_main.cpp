#include "rb4pc/runtime/mogg_parser.hpp"
#include "rb4pc/runtime/mogg_mix.hpp"
#include "rb4pc/runtime/vorbis_decoder.hpp"
#include "rb4pc/runtime/vorbis_playback_queue.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
int main(int argc, char* argv[]) {
    if (argc != 2 && !(argc == 3 && std::string(argv[2]) == "--verify-stream") &&
        !(argc == 4 && (std::string(argv[2]) == "--extract-ogg" ||
                        std::string(argv[2]) == "--verify-mix"))) {
        std::cerr << "Usage: " << argv[0]
                  << " <input.mogg> [--verify-stream | --verify-mix <mogg.dta> | --extract-ogg <output.ogg>]\n";
        return 2;
    }
    try { const auto h = rb4pc::runtime::parse_mogg_header(argv[1]);
        const auto vorbis = rb4pc::runtime::inspect_ogg_vorbis(argv[1], h.ogg_offset);
        std::cout << "MOGG size: " << h.file_size << " bytes\nVersion: " << h.version << "\nOgg offset: " << h.ogg_offset << "\nEmbedded Ogg: " << (h.embedded_ogg ? "yes" : "no") << '\n';
        std::cout << "Vorbis channels: " << static_cast<unsigned int>(vorbis.channels)
                  << "\nSample rate: " << vorbis.sample_rate
                  << " Hz\nOgg pages: " << vorbis.page_count
                  << "\nFinal granule: " << vorbis.final_granule
                  << "\nDuration: " << vorbis.duration_seconds << " seconds\n";
        const auto pcm = rb4pc::runtime::decode_vorbis_preview(argv[1], h.ogg_offset, 44'100);
        std::cout << "Decoded preview frames: " << pcm.frames
                  << "\nDecoded peak amplitude: " << pcm.peak_amplitude
                  << "\nFinite PCM samples: " << (pcm.finite_samples ? "yes" : "no") << '\n';
        if (argc == 3) {
            rb4pc::runtime::VorbisPlaybackQueue queue(argv[1], h.ogg_offset);
            queue.play();
            bool finite_stream = true;
            while (!queue.source_exhausted()) {
                const auto block = queue.queue_next(4096);
                if (block.frame_count() == 0) throw std::runtime_error("Vorbis stream decoder stalled");
                for (const auto sample : block.samples) finite_stream = finite_stream && std::isfinite(sample);
                queue.mark_completed(block.frame_count());
            }
            std::cout << "Streamed frames: " << queue.transport().completed_frames()
                      << "\nTransport seconds: " << queue.transport().seconds()
                      << "\nFinite streamed PCM: " << (finite_stream ? "yes" : "no") << '\n';
            if (!finite_stream || !queue.transport().finished()) return 1;
            rb4pc::runtime::VorbisPlaybackQueue seek_check(argv[1], h.ogg_offset);
            const auto seek_target = seek_check.transport().total_frames() / 2;
            seek_check.seek_frame(seek_target);
            seek_check.play();
            const auto seek_block = seek_check.queue_next(4096);
            if (seek_block.first_frame != seek_target || seek_block.frame_count() == 0) return 1;
            seek_check.mark_completed(seek_block.frame_count());
            std::cout << "Seek verification frame: " << seek_target << '\n';
        }
        if (argc == 4 && std::string(argv[2]) == "--verify-mix") {
            const auto metadata = rb4pc::runtime::parse_mogg_mix_metadata(argv[3], vorbis.channels);
            rb4pc::runtime::VorbisPlaybackQueue queue(argv[1], h.ogg_offset);
            queue.play();
            float mixed_peak = 0.0F;
            float pre_clamp_peak = 0.0F;
            std::uint64_t clipped_samples = 0;
            bool finite_mix = true;
            while (!queue.source_exhausted()) {
                const auto block = queue.queue_next(4096);
                if (block.frame_count() == 0) throw std::runtime_error("Vorbis mix decoder stalled");
                const auto mixed = rb4pc::runtime::mix_mogg_to_stereo(block, metadata);
                mixed_peak = std::max(mixed_peak, mixed.peak_amplitude);
                pre_clamp_peak = std::max(pre_clamp_peak, mixed.pre_clamp_peak_amplitude);
                clipped_samples += mixed.clipped_samples;
                for (const auto sample : mixed.samples) finite_mix = finite_mix && std::isfinite(sample);
                queue.mark_completed(block.frame_count());
            }
            std::cout << "Mix stems: " << metadata.stems.size()
                      << "\nMixed frames: " << queue.transport().completed_frames()
                      << "\nStereo peak amplitude: " << mixed_peak
                      << "\nPre-clamp peak amplitude: " << pre_clamp_peak
                      << "\nClipped stereo samples: " << clipped_samples
                      << "\nFinite stereo PCM: " << (finite_mix ? "yes" : "no") << '\n';
            if (!finite_mix || !queue.transport().finished()) return 1;
        }
        if (argc == 4 && std::string(argv[2]) == "--extract-ogg") {
            rb4pc::runtime::extract_mogg_ogg(argv[1], h, argv[3]);
            std::cout << "Extracted Ogg: " << argv[3] << '\n';
        }
        return h.embedded_ogg ? 0 : 1;
    } catch (const std::exception& e) { std::cerr << "rb4pc-mogg-probe: " << e.what() << '\n'; return 1; }
}
