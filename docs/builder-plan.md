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
