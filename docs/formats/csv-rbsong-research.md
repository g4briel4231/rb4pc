# Native `csv_pc` and `rbsong` research

## Verified ARK storage layer (v0.2280.0)

One RBVR PC ARK volume contains a protected logical stream. Its stored size is 60 bytes larger than the size declared by the HDR. A key-material block at the end of the volume derives an initial byte key; each stored byte is then decoded with a position-dependent rolling XOR key. RB4PC independently implements detection, bounded random-access decoding, and logical extraction without loading GameArchives or LibForge at runtime.

The result was checked against `GameArchives.Ark.ArkPackage.GetFile(...).GetStream()` for two records from a user-owned installation:

| Record | Logical bytes | SHA-256 | Result |
| --- | ---: | --- | --- |
| `gtrrhythm_mutes.csv_pc` | 121335 | `2fdafc45fd38e4889847c1a179b421cb37ffc21130b07fdfd039cea434b97ba9` | Byte-identical |
| `aintitfun.rbsong` | 32063 | `327ef39c607509ae64829be6752154175a884354b84b41aa7bc4776920e7d69a` | Byte-identical |

Normal unprotected ARK volumes still use direct bounded copying. Protected-volume detection validates the descriptor, key-material bounds, and logical range before reading.

## Logical `csv_pc`

The verified logical layout is:

1. little-endian `u32` format version `1`;
2. little-endian `u32` encoding value `2`;
3. one-byte separator `,`;
4. little-endian `u32` string-table size;
5. one NUL-terminated shared UTF-8 string table;
6. header count followed by `u32` offsets into that table;
7. row count, then a cell count and table offsets for each row.

The strict reader rejects unsupported header values, out-of-range offsets, missing NUL terminators, impossible counts, truncated data, and trailing bytes. All 65 corpus records parse successfully. The corpus contains 12 measured table shapes: six 54-header song-table row-count variants and six localization shapes with 5–7 headers. The protected validation sample produces 54 headers and 377 rows, matching LibForge. Inspect a logical file with:

```text
rb4pc-builder <input.csv_pc> --inspect-native-csv <report.json>
```

This is read-only support; no `csv_pc` writer is claimed yet.

## Logical `rbsong`

The structural reader currently accepts observed entity-resource versions 12–17. It reads the version, inline-layer names, version-gated prefix fields, entity payload offset, and entity version. Verified samples are:

- protected installed record: resource version 17, entity offset 20, entity version 30;
- loose ForgeTool-generated record: resource version 14, entity offset 12, entity version 20.

The loose version-14 sample also round-trips byte for byte through LibForge: 9491 bytes, SHA-256 `8477196eced23483a91c29f16000d14ce80716fa25a5cdc68e87d453842b97f`.

```text
rb4pc-builder <input.rbsong> --inspect-native-rbsong <report.json>
```

Version 0.2320.0 adds an independent structural tree reader for the verified single-layer layout. It walks entity layers, game objects, empty/sentinel entries, components, property definitions, recursive struct/array type descriptors, and every observed value encoding to preserve stream alignment. Reports expose names and type shapes, but deliberately do not expose or assign semantics to property values yet.

Across the 76-record installed corpus, 70 records parse completely into eight observed structural shapes spanning resource versions 14–17, entity/layer versions 20, 23, and 30, two or four object slots, and 31–34 root property definitions. Every parsed root contains the four components `Editor`, `EntityHeader`, `RBSongMetadata`, and `RBVenueAuthoring`. Six tutorial records are rejected because their component counts become impossible at the same point where LibForge fails with an arithmetic overflow; they are retained as an explicit unsupported/corrupt cohort rather than guessed through.

Inline-resource payloads are still outside RB4PC's tree report, and no `rbsong` writer is claimed.

## Next evidence steps

1. Run the logical CSV reader across the complete 65-record corpus and classify schema shapes.
2. Validate tree/component/property shapes across the complete 76-record `rbsong` corpus.
3. Decode and classify inline resources and property values without assigning unsupported meanings.
4. Add semantic readers only after independent cross-sample validation.
5. Add writers only after byte-level and consumer-level validation of every supported structure.