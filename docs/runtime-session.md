# Integrated runtime session

Version 0.9.0 creates one startup boundary for the previously separate pieces.
`RuntimeSession` validates the HDR/ARK source, builds the conversion inventory,
mounts the read-only VFS, discovers assets by prefix, and instantiates validated
WAVE metadata through the runtime loader.

The `rb4pc-runtime` probe reports whether the source is ready, how many records
are mounted, how many RIFF records are currently understood, and up to five PCM
WAVE samples under an optional prefix. No window, renderer, audio device,
controller input, gameplay loop, or proprietary executable code is present.

Version 0.10.0 adds a machine-readable diagnostic report whose capability flags
explicitly keep audio playback, rendering, and gameplay false until those
subsystems genuinely exist.
