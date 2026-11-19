# RB4PC Builder plan schema

Version 0.6.0 establishes a read-only planning boundary between source
inspection and future conversion. `rb4pc-builder` accepts a user-supplied HDR
path and writes schema version 1 JSON containing:

- platform hint and decoded HDR version;
- total, present, exact-size, and usable-size ARK counts;
- total and addressable record counts;
- path-hash validation status;
- readiness for conversion planning.

An ARK is usable when it exists and is at least as large as its declared HDR
size. Exact-size counts remain separate, preserving the known 60-byte excess in
the available Rock Band VR `main_pc_1.ark` sample rather than hiding it.

Readiness requires HDR version 10, a validated hash index, at least one ARK,
every ARK usable, and every record resolved within one declared volume. It does
not claim that every internal asset format is understood or PC-compatible.

The plan contains no game content, and this version performs no extraction,
conversion, runtime generation, or source-file modification.

## Canonical song layout

Observed ForgeToolGUI installations place each custom song directly in the Rock
Band VR installation root as `<shortname>_pc.hdr` plus
`<shortname>_pc_0.ark`. The HDR is version 10 and declares the matching ARK.
Internal assets may still use paths containing `/songs/`; that namespace must
not be confused with the host installation layout. Builder short names accept
only letters, digits, `_`, and `-` before package filenames are generated.

Version 0.13.0 can scan this flat installation layout. A package is ready when
its HDR is version 10, its hash index validates, it declares exactly the
expected `<shortname>_pc_0.ark`, and that ARK is present with at least the
declared size.

## Complete song-session plan

Version 0.46.0 adds a read-only plan joining a user-supplied standard MIDI,
compatible MOGG v10, and `mogg.dta`. Readiness requires a playable expert guitar
chart, valid Vorbis timing, complete per-channel mix metadata, a named guitar
stem, and a chart timeline that does not extend more than 250 ms beyond the
audio. The JSON records source hashes and derived properties, not song content.

The reference `abcdefu.rbmid_ps4` was converted to standard MIDI by ForgeTool
and planned against its matching MOGG and mix metadata. Builder found 657 expert
guitar chords ending at 174.688 seconds, while the 10-channel audio ends at
176.865 seconds. All five stems were present, including guitar, and the resulting
real-file plan reported `readyForSongSession: true`.
