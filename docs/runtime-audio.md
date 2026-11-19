# Runtime PCM WAVE loader

Version 0.8.0 loads RIFF/WAVE metadata through the read-only archive VFS. It
walks logical chunks, requires the observed PCM format tag 1, and exposes channel
count, sample rate, byte rate, block alignment, bit depth, data range, frame
count, and duration.

PCM bytes remain in the user's ARK files and are read only in caller-requested,
record-bounded ranges. This milestone does not provide an audio device, mixer,
resampling, streaming queue, MOGG decoding, or synchronization with gameplay.

Version 0.8.1 adds `PcmStream`, which exposes frame-aligned reads and seek while
tracking the current and remaining frame counts. Reads clamp at end of stream.

Version 0.12.0 adds the first operating-system output backend. On Windows,
`rb4pc-audio-play` submits one complete validated PCM WAVE buffer through
`waveOut` and waits for completion. Playback is explicit and is never invoked by
the automated test suite. Mixing, resampling, MOGG decoding, low-latency queues,
and gameplay synchronization remain future work.

Version 0.45.0 adds a separate compatible-MOGG path on Windows. It incrementally
decodes Vorbis, applies `mogg.dta` stem/pan/volume metadata, limits normalized
stereo float samples, converts them to signed 16-bit PCM, and maintains two
queued `waveOut` buffers. Completed device buffers advance the same frame clock
used by gameplay. Automated tests cover conversion and transport invariants but
do not open the user's audio device.

The `rb4pc-mogg-play` executable provides the explicit hardware-validation
entry point. Its presence and compilation do not by themselves claim that a
particular sound device completed a full song; that distinction remains part of
the release evidence.
