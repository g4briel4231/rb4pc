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

Version 0.10.0 adds a machine-readable diagnostic report whose capability flags
explicitly keep audio playback, rendering, and gameplay false until those
subsystems genuinely exist.
