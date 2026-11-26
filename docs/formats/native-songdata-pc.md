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

This proves reading and writing for the observed fixed-size layout. Version 0.2240.0 adds explicit new-record rules below. `rbsong` and `csv_pc` remain separate formats.
## New-record conversion rules (v0.2240.0)

```text
rb4pc-builder <song.ini> --convert-native-songdata <new-output.songdta_pc>
```

Required keys are `name`/`title`, `artist`, `song_id`, and `song_length` (milliseconds). `shortname` may be supplied or is derived from the directory. Supported optional keys include `album`, `genre`, `year`, `original_year`, `preview_start_time`, `preview_end_time`, `album_track_number`, `type`, `version`, `game_origin`, `diff_guitar`, `diff_bass`, `diff_vocals`, `diff_drums`, `diff_band`, `diff_keys`, `diff_real_keys`, `tutorial`, `album_art`, `cover`, `vocal_gender`, `medium`, `freestyle_vocals`, `vocal_parts`, `flags`, and `fake`.

Defaults are deterministic: type 11, version -1, origin `ugc_plus`, numeric ranks/flags zero, album art true, other booleans false, and preview end `min(preview start + 30000, song length)`. The preview range must fit inside the song. Every defaulted field is listed in the manifest. The generated file is reread by RB4PC; an independent LibForge read of the validation fixture recovered the same identity, timing, ranks, type, and origin.