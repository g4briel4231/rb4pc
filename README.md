# RB4PC

RB4PC is an experimental, clean-room effort to build independent PC tooling and
runtime components compatible with assets supplied by users from their own
legally obtained copy of Rock Band 4.

The repository does **not** contain game executables, songs, ARK/HDR archives,
console libraries, or other proprietary game content. Do not add those files to
the repository.

## Current milestone: v0.29.0 Five-Fret Gameplay Core

The first executable is `rb4pc-forge-inspector`, a read-only tool for recording
verifiable properties of candidate files. It reports the file size, SHA-256,
first 32 bytes, a filename-derived platform hint, and the ordered ARK volumes
associated with an HDR file. Format parsing will only be added as findings are
tested and documented.

The report also records Shannon entropy globally and per 64 KiB block, zero-byte
counts, and exactly repeated full blocks. These measurements describe byte
distributions; they do not by themselves prove compression or encryption.

Version 10 HDR files are decoded with the verified Park-Miller XOR stream. The
report lists every ARK declared by the header—including patch volumes—and checks
each declared size against the local file without modifying game data.

The decoder also reads per-archive tags and the complete internal file-record
table. Version 0.2.2 also decodes and validates the final path-hash bucket table
and each record's collision-chain link.

Version 0.3.0 begins read-only ARK access by translating each record's global
offset into a declared volume index and a local offset. A record is resolved
only when its complete byte range stays inside one volume; cross-volume and
out-of-range spans are rejected.

Version 0.3.1 adds the optional payload catalog. It samples at most 16 bytes
from each non-empty record, groups internal filename extensions, recognizes a
small set of established magic values, and retains representative paths for
each signature group. Short zlib and gzip matches are labeled as candidates
rather than treated as proof of compression.

## Build

Requirements:

- CMake 3.25 or newer
- A C++20 compiler
- Ninja when using the included preset

```powershell
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Run the inspector against a file stored outside this repository:

```powershell
./build/default/rb4pc-forge-inspector.exe "../path/to/main_ps4.hdr" --output report-ps4.json
```

Look up an exact internal path through the HDR hash index and read a bounded
32-byte preview directly from its ARK volume:

```powershell
./build/default/rb4pc-forge-inspector.exe "../path/to/main_ps4.hdr" --find "path/inside/archive"
```

This operation is read-only. It does not extract the complete record or alter
the user's HDR/ARK files.

Catalog extensions and recognized payload signatures using a maximum 16-byte
sample from each non-empty record:

```powershell
./build/default/rb4pc-forge-inspector.exe "../path/to/main_ps4.hdr" --catalog
```

Unknown signatures remain explicitly labeled `unknown`; the tool does not infer
compression or a file type solely from an internal filename extension.

Extract one exact internal path to an explicit new destination:

```powershell
./build/default/rb4pc-forge-inspector.exe "../path/to/main_ps4.hdr" `
  --extract "path/inside/archive" --destination "./output.bin"
```

The extractor refuses to overwrite an existing destination, copies through a
temporary file, validates the record against the actual ARK size, and reports
the extracted size and SHA-256. The internal path is never used as a host
filesystem destination.

Extract a filtered set while preserving its safe internal hierarchy:

```powershell
./build/default/rb4pc-forge-inspector.exe "../path/to/main_ps4.hdr" `
  --extract-prefix "songs/example/" --extract-extension ".mogg" `
  --destination-dir "./selected-assets"
```

Prefix and extension filters may be used separately or together. Zero-size
markers are excluded. The destination directory must not already exist, and
the completed directory contains `rb4pc-manifest.json` with each internal path,
relative output path, size, and SHA-256. Absolute paths, drive-qualified names,
empty components, `.` and `..` components are rejected.

Version 0.4.0 begins structural parsing of payload formats with RIFF. During
`--catalog`, every RIFF record's logical size is compared with its HDR record
size and its top-level chunk table is walked with alignment and boundary
validation. Reports include valid/invalid counts, form types, chunk identifiers,
and whether any trailing allocation region is zero or nonzero.

Version 0.4.1 decodes the standard 16-byte prefix of each WAVE `fmt ` chunk and
catalogs format tags, channel counts, sample rates, block alignment, byte rates,
and bits per sample without decoding or playing the audio payload.

Version 0.4.2 records the first two little-endian 32-bit values in each FEV
`FMT ` chunk and compares their distributions across platforms. Their semantics
remain explicitly unknown until corroborated by additional evidence.

Version 0.5.0 adds the first asset conversion operation. `--convert-riff`
exports the structurally validated logical RIFF range to a new file. If the HDR
record contains trailing bytes, they are preserved in an adjacent
`.rb4pc-trailing.bin` sidecar with a separate SHA-256 instead of being silently
discarded.

```powershell
./build/default/rb4pc-forge-inspector.exe "../path/to/main_ps4.hdr" `
  --convert-riff "path/inside/archive.wav" --destination "./converted.wav"
```

The destination and any sidecar must not already exist. Conversion is read-only
with respect to the user's HDR/ARK source files.

Version 0.5.1 re-parses every converted RIFF before reporting success and emits
an adjacent `.rb4pc-conversion.json` manifest containing the internal source
path, form type, logical and record sizes, output hash, and optional sidecar hash.

Version 0.6.0 introduces `rb4pc-builder` as a separate executable. Its first
operation is deliberately non-destructive: it validates HDR version 10, the
path hash index, ARK presence and usable sizes, and complete record addressing,
then writes a versioned JSON conversion plan. It does not yet build a runtime or
copy proprietary assets.

```powershell
./build/default/rb4pc-builder.exe "../path/to/main_ps4.hdr" --plan "./build-plan.json"
```

Version 0.6.1 adds a conversion inventory to the plan: structurally validated
RIFF records and their `WAVE`/`FEV ` split. This measures currently supported
work without implying that unknown formats are convertible.

Version 0.7.0 introduces a runtime-facing virtual file system over the user's
HDR/ARK set. It resolves exact internal paths through the verified hash index,
reports logical record sizes, and performs bounded range reads without prior
extraction. `rb4pc-runtime-probe` mounts a set and reads a 32-byte preview.

```powershell
./build/default/rb4pc-runtime-probe.exe "../path/to/main_ps4.hdr" "path/inside/archive"
```

Version 0.7.1 adds bounded prefix enumeration to the VFS so runtime loaders can
discover related assets without exposing host filesystem paths.

Version 0.8.0 adds the first runtime asset loader. It parses PCM WAVE metadata
through VFS range reads, locates the `data` chunk, calculates frame count and
duration, and exposes bounded PCM reads without extracting the source file.
`rb4pc-audio-probe` reports this metadata; audio playback is not implemented yet.

Version 0.8.1 adds a frame-aligned PCM stream cursor with bounded reads, seek,
end-of-stream clamping, and overflow checks. It remains independent of any
operating-system audio API.

Version 0.9.0 integrates source validation, conversion inventory, the archive
VFS, asset discovery, and the PCM WAVE loader into `RuntimeSession`. The new
`rb4pc-runtime` executable boots a user-supplied set and reports readiness plus
sample audio assets. It is an integration probe, not yet gameplay.

```powershell
./build/default/rb4pc-runtime.exe "../path/to/main_ps4.hdr" "ps4/fusion/samples/"
```

Version 0.10.0 adds versioned runtime diagnostics that explicitly distinguish
implemented capabilities from absent playback, rendering, and gameplay.

Version 0.11.0 adds `--music-catalog`, a bounded header census for compiled MIDI,
MIDI-song, RBMID, and MOGG records. It reports counts, size ranges, distinct
four-byte prefixes, and whether standard `MThd` or `OggS` headers are exposed.

Version 0.12.0 adds explicit, blocking PCM playback through the Windows
`waveOut` API. `rb4pc-audio-play` accepts one validated WAVE path; no sound is
played during automated tests. Other platforms report that no backend exists.

Version 0.12.1 corrects the custom-song installation model from real
ForgeToolGUI output: each song is a flat root-level pair named
`<shortname>_pc.hdr` and `<shortname>_pc_0.ark`. `songs/` remains an internal
archive path, not a host installation directory.

Version 0.13.0 scans a Rock Band VR installation for root-level custom-song
package pairs, validates each HDR v10 and matching ARK, and writes a catalog
without copying or modifying the installed songs.

Version 0.14.0 adds an XInput backend on Windows and `rb4pc-input-probe`, which
reports four controller slots and raw button, trigger, and stick state. Device
diagnostics are separate from gameplay mappings.

Version 0.15.0 adds a platform-neutral 32-bit software framebuffer and an
explicit Win32 window backend. `rb4pc-window` displays a generated diagnostic
gradient; automated tests exercise only framebuffer memory and never open a
window.

Version 0.16.0 adds clipped filled/bordered rectangles and a deterministic,
wrapping menu-selection model. Text and game assets are not rendered yet.

Version 0.17.0 adds a deterministic pausable gameplay clock and provisional
note-judgement windows. These synthetic rules are infrastructure, not recovered
Rock Band scoring behavior and not yet connected to a chart format.

Version 0.18.0 creates a dry-run installation plan for validated custom-song
packages. It records source, root-level destination, size, total bytes, and
collisions without copying any file.

Version 0.19.0 adds a deterministic shell state machine for boot, main menu,
song browser, settings, diagnostics, and quit. It is tested without opening a
window and does not yet display text or launch gameplay.

Version 0.20.0 integrates source validation, runtime session, shell state,
software UI rendering, and the Win32 window in `rb4pc-prealpha`. The visual
panel is diagnostic and contains no text, charts, gameplay, or original assets.

Version 0.21.0 records the internal chart-related composition of every Forge
custom-song package: compiled MIDI, MIDI-song, RBMID, MOGG, and song metadata.
This validates required content without claiming those containers are decoded.

Version 0.22.0 identifies the custom-song namespace as
`pc/songs_download/<shortname>/`, adds bounded VFS prefix listing to the runtime
probe, and records namespace coverage per package. Binary DTA payloads remain
opaque until their transformation is verified.

Version 0.23.0 adds a strict standard-MIDI header parser and probe for known
source charts. This creates a trustworthy input-side reference for future
MIDI-to-RBMID correlation; it does not yet decode `.rbmid_pc`.

Version 0.24.0 walks MIDI tracks, running status, channel/meta/SysEx events,
track names, note counts, tempo events, and end ticks. This remains the known
input side of the RBMID comparison.

Version 0.25.0 pairs note-on and note-off events by channel and key, preserving
start/end ticks and velocity while reporting unmatched releases and dangling
notes. This produces bounded chart primitives without assigning instrument,
difficulty, lane, or scoring semantics yet.

Version 0.26.0 maps the established five-fret note ranges for easy, medium,
hard, and expert guitar/bass charts. Simultaneous gems become one chord with a
five-bit lane mask and a bounded sustain end tick. Drum and vocal semantics are
not inferred by this mapper.

Version 0.27.0 preserves standard-MIDI tempo changes and converts absolute ticks
to seconds across the complete tempo map. The default 120 BPM applies before
the first tempo event; zero tempos and unsupported SMPTE divisions are rejected.

Version 0.28.0 projects five-fret chords onto that tempo timeline and exposes a
deterministic visible-note query with configurable past and future windows.
Sustains remain visible through their end time; negative time inputs are rejected.

Version 0.29.0 adds a deterministic five-fret gameplay cursor. It expires late
notes, accepts only an exact five-lane chord mask inside the provisional 100 ms
window, applies the existing timing judgement, and records hit/miss counts and
score. These are project-defined test rules, not recovered Rock Band scoring.

```powershell
./build/default/rb4pc-builder.exe "../Rock Band VR" `
  --scan-custom-songs "./custom-songs.json"
```

Reports use a versioned JSON schema so future research remains reproducible.
The platform hint is based only on the filename suffix and is not presented as
evidence about the file's internal format.

## Repository layout

- `include/rb4pc`: public C++ interfaces
- `src/inspector`: ForgeInspector implementation and command-line application
- `src/builder`: RB4PC Builder planning and command-line application
- `tests`: automated tests using synthetic, non-proprietary data
- `docs/formats`: evidence-backed format notes (planned)
- `docs/research`: reproducible static-analysis notes without proprietary samples
- `docs/builder-plan.md`: current Builder plan schema and readiness rules

## Legal notice

RB4PC is an independent community project and is not affiliated with or endorsed
by Harmonix, Epic Games, Electronic Arts, Sony, or Microsoft. Users are
responsible for complying with applicable laws and licenses. No ownership of the
Rock Band trademarks or original game content is claimed.

