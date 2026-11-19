# Integrated runtime session

Version 0.9.0 creates one startup boundary for the previously separate pieces.
`RuntimeSession` validates the HDR/ARK source, builds the conversion inventory,
mounts the read-only VFS, discovers assets by prefix, and instantiates validated
WAVE metadata through the runtime loader.

The `rb4pc-runtime` probe reports whether the source is ready, how many records
are mounted, how many RIFF records are currently understood, and up to five PCM
WAVE samples under an optional prefix. No window, renderer, audio device,
controller input, gameplay loop, or proprietary executable code is present.

## Complete headless song session

Version 0.47.0 joins the standard-MIDI expert guitar chart, streaming compatible
MOGG decoder, metadata-driven stem mixer, frame transport, and five-fret
gameplay in one verifier. It advances gameplay from completed audio frames and
uses exact-timestamp deterministic autoplay to expose synchronization errors.

The matching `abcdefu` reference completed 1,905 audio blocks and 7,799,738
frames (176.865 seconds). All 657 expert guitar chords were hit for 65,700
points with zero misses; PCM remained finite, and both transport and gameplay
reported completion. This proves a full headless session, not yet interactive
human play or hardware audio completion.

## Interactive audio-clocked session

Version 0.48.0 introduces `rb4pc-song-play`. The Windows audio backend runs on a
cancellable worker while the window thread polls XInput slot zero and renders
the highway. Only completed device frames advance `FiveFretSession`; wall-clock
time is not used as a competing gameplay clock. Buffers are 1,024 frames
(approximately 23.2 ms at 44.1 kHz), below the provisional 100 ms hit window.

Closing the window requests cancellation, resets queued `waveOut` buffers, and
joins the audio worker. Pause input is disabled in this entry point because
pausing gameplay without pausing the device would violate synchronization.
Compilation and deterministic subsystems are automated; a human/device run is
still required before claiming hardware-interactive validation.

## Verified song library

Version 0.53.0 scans the conventional `songs/` directory in short-name order.
Each child is resolved and hash-verified independently. Invalid entries include
their diagnostic error and do not prevent valid songs from loading. This is the
runtime data model for the upcoming song browser; it does not ingest any song
content into the executable or repository.

Version 0.10.0 adds a machine-readable diagnostic report whose capability flags
explicitly keep audio playback, rendering, and gameplay false until those
subsystems genuinely exist.
