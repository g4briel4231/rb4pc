# Native PC songdata (`songdta_pc`)

RB4PC v0.2200.0 independently reads and writes the observed fixed-size PC songdata layout.

## Verified layout

The payload is exactly 1202 bytes. The implemented fields are type, song ID, version, game origin, preview boundaries, name, artist, album, album track, album/original years, genre, song length, seven instrument/band ranks, tutorial/art/cover flags, vocal gender, medium, freestyle-vocal flag, vocal parts, flags, fake flag, and short name. Fixed-width strings and required zero padding are validated strictly.

## Commands

```text
rb4pc-builder <input.songdta_pc> --inspect-native-songdata <report.json>
rb4pc-builder <input.songdta_pc> --roundtrip-native-songdata <new-output.songdta_pc>
```

Round-trip output is written temporarily, parsed again, compared field by field, hashed, and published only when its SHA-256 equals the source. Existing destinations are never overwritten.

## Independent evidence

A user-owned Rock Band VR fixture was read and rewritten independently by LibForge and RB4PC. All three 1202-byte files were byte-identical with SHA-256 `fdae74acf355f97f50c01b5342f0774cece050c4dd7529f9554167d554aea3f7`.

## Boundary

This proves reading and writing for the observed fixed-size layout. Creating a complete new native record from `song.ini` still requires explicit rules for song IDs, ranks, duration, flags, and other fields. `rbsong` and `csv_pc` are separate formats.