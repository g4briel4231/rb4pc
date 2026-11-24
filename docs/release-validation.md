# Release validation

## v1.0.0 acceptance boundary

RB4PC 1.0.0 is accepted when the release build can complete the following
workflow with user-owned assets stored outside the repository:

1. Parse a standard MIDI chart and import an expert five-fret guitar track.
2. Parse a version-10 MOGG containing embedded Vorbis audio.
3. Parse its `mogg.dta` channel layout and construct the stereo mix.
4. Convert a supported linear PS4 texture to the canonical PC container.
5. Assemble `songs/<shortname>/` with a versioned manifest and SHA-256 hashes.
6. Reopen that directory and verify its names, files, manifests, and hashes.
7. Decode the complete song, advance audio transport, and finish deterministic
   gameplay without a miss, stalled decoder, non-finite sample, or unfinished
   transport.

The v1.0.0 release candidate passed this workflow on Windows x64 with a real
10-channel, 44.1 kHz MOGG, supported PS4 artwork, and a standard MIDI chart with
761 expert-guitar chords. The decoded song contained 7,497,000 audio frames and
the verifier completed all 761 notes. These measurements identify the tested
format family; no source song, game archive, or proprietary payload is stored
in the repository or release package.

## Reproduction

Build and test the release:

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Create a local validation directory and assemble a song. All four source paths
must point to files obtained or created by the user:

```powershell
rb4pc-builder.exe <notes.mid> --assemble-song-with-art `
  <song.mogg> <song.mogg.dta> <art.png_ps4> <output-root> <shortname>

rb4pc-song-verify.exe <output-root>/songs/<shortname>
```

A passing verifier reports the imported track, decoded audio frames, chart
chords, autoplay results, finite-PCM status, and completion of both transport
and gameplay.

## Supported in 1.0.0

- Windows x64 release packaging;
- standard MIDI charts with five-fret guitar data;
- version-10 MOGG files with a supported embedded Vorbis stream;
- `mogg.dta` track, pan, and volume metadata;
- linear magic-6/version-12 PS4 artwork conversion;
- manifest-backed song directories under `songs/`;
- keyboard and XInput gameplay, artwork rendering, volume, and latency config.

## Explicitly outside this milestone

- distributing Rock Band 4, its songs, executables, archives, or console code;
- executing or translating the original PS4 executable;
- direct gameplay decoding of PS4 RBMID charts;
- converting every observed PS4 texture family;
- claiming compatibility with every song or peripheral.

Unsupported inputs must fail with a diagnostic instead of being silently
treated as compatible.
