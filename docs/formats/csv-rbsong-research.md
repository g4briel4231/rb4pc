# `csv_pc` and `rbsong` research boundary

## Corpus

RB4PC extracted 65 `csv_pc` records and 76 `rbsong` records from a user-owned Rock Band VR `main_pc.hdr`, preserving paths and hashes. Six localization `csv_pc` files were directly readable by LibForge, with 5–7 columns and 866–3890 rows. The other 59 CSV records did not expose the same logical header after raw ARK extraction, which indicates an additional storage transformation or protection layer.

The 76 ARK-extracted `rbsong` records likewise did not begin with the logical entity-resource header expected by LibForge. They must not be passed to a logical-format writer until that storage layer is identified.

## Loose native evidence

A loose ForgeTool-generated `rbsong` loaded as entity-resource version 14 with one inline layer. LibForge rewrote it byte for byte: 9491 bytes, SHA-256 `8477196eced23483a91c29f16000d14ce80716fa25a5cdc68e87d453842b97f` (case-insensitive).

## Next evidence steps

1. Compare raw ARK records with independently obtained logical exports of the same paths.
2. Identify whether the differing bytes are encryption, compression, or another container layer.
3. Implement read-only decoders and corpus tests before any writer.
4. For `rbsong`, map entity types, property definitions, inline resources, and values across multiple loose samples before constructing new trees.

No native writer is claimed for either family yet.