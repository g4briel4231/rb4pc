# Format research

This directory will contain reproducible observations about Forge-related file
formats. Every claim should identify the input version, byte offsets, endianness,
and a repeatable verification method. Do not commit samples copied from a game.

Static executable research is kept separately in
[`../research/rbvr-static-analysis.md`](../research/rbvr-static-analysis.md).

Verified format notes:

- [`hdr-v10.md`](hdr-v10.md): stream transformation and decoded prefix
- [`payload-catalog.md`](payload-catalog.md): bounded signature and extension census
- [`riff-payloads.md`](riff-payloads.md): RIFF forms, chunks, and record-size validation
- [`riff-conversion.md`](riff-conversion.md): lossless logical RIFF export policy
- [`music-payloads.md`](music-payloads.md): compiled MIDI and MOGG header census

