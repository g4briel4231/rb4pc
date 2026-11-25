# Verified canonical song metadata

RB4PC v0.2040.0 can convert understood textual fields from a `song.ini` file into deterministic canonical RB4PC JSON.

```text
rb4pc-builder <song.ini> --convert-song-metadata <new-metadata.json>
```

The converter requires `name` (or `title`) and `artist`. It also understands `shortname`, `album`, `genre`, `year`, and `preview_start_time` in milliseconds. If `shortname` is absent, it is derived from the song directory name.

The operation never overwrites an existing destination. It writes to a temporary file, rereads the JSON, compares every supported field with the parsed source, calculates source and destination SHA-256 values, and only then publishes the JSON plus `<new-metadata.json>.rb4pc-conversion.json`.

## Scope

This is an RB4PC-owned interchange format (`schemaVersion: 1`). It is not a writer for Harmonix `songdta_pc`, `moggsong_dta_pc`, `csv_pc`, `rbsong`, HDR, or ARK data. Those native formats remain read-only research targets until their semantics are independently verified.