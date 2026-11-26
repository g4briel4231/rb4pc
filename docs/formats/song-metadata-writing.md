# Verified song metadata writing

RB4PC v0.2160.0 supports verified canonical conversion and one narrowly verified native PC metadata family.

## Canonical single-file conversion

```text
rb4pc-builder <song.ini> --convert-song-metadata <new-metadata.json>
```

The converter requires `name` (or `title`) and `artist`. It also understands `shortname`, `album`, `genre`, `year`, and `preview_start_time` in milliseconds. If `shortname` is absent, it is derived from the song directory name. Output is written temporarily, reread field by field, hashed, and published with a conversion manifest. Existing files are never overwritten.

## Transactional batch conversion

```text
rb4pc-builder <song-root> --convert-song-metadata-batch <new-output-directory>
```

The batch preflights every recursive `song.ini` before writing anything. Invalid fields and case-insensitive duplicate short names reject the entire operation. Each result is reread and hash-checked before the temporary output tree is renamed into place. `batch-summary.json` records paths, fields, and hashes.

## Native `moggsong_dta_pc`

```text
rb4pc-builder <file.mogg> --write-native-moggsong <file.rbmid> <new-output.moggsong_dta_pc>
```

This writer encodes exactly the verified `mogg_path` and `midi_path` binary-DTA structure. It rereads both fields and verifies the output hash before publication. The reference fixture produced identical bytes and SHA-256 (`beacd4d195d286473d0cd3601834be9339d1ab8074b4d19aa1aabf0be0f5771f`) in three independent paths: the installed Rock Band VR file, ForgeTool `dta2b`, and RB4PC.

## Scope boundary

The canonical JSON is an RB4PC-owned interchange format. Native `moggsong_dta_pc` support is deliberately limited to the two verified path fields. RB4PC does not yet claim complete `songdta_pc`, `csv_pc`, or `rbsong` writing, nor native HDR/ARK integration for these results.