# Forge HDR version 10: verified layout

This document describes only fields reproduced from the three available header
sets. Integers observed so far are little-endian after decoding.

## Stream transformation

The first four bytes of an HDR file are an unencoded signed 32-bit seed. The
remaining bytes are XORed one at a time with the low byte produced by a
Park-Miller minimal-standard generator.

Seed normalization observed at `rbvr.exe` VA `0x140C0F430`:

1. Interpret the first four bytes as a signed 32-bit integer.
2. Replace zero with 1.
3. Negate a negative value.

Generator observed at VA `0x140C0F450`:

```text
quotient = state / 127773
next = 16807 * (state % 127773) - 2836 * quotient
if next <= 0: next += 2147483647
state = next
plain_byte = encoded_byte XOR (state & 0xff)
```

The seed itself is not part of the decoded payload.

## Verified prefix layout

| Decoded offset | Size | Meaning | Evidence |
|---:|---:|---|---|
| `0x00` | 4 | Header version | Value 10 in PC, PS4, and Xbox One samples |
| `0x04` | 4 | Unknown flag/value | Value 1 in all three samples |
| `0x08` | 16 | Unknown 16-byte value, possibly GUID | Distinct binary value in each sample |
| `0x18` | 4 | ARK count | 5 PC, 62 PS4, 58 Xbox One |
| `0x1C` | 4 | ARK-size array count | Same value as ARK count in all samples |
| `0x20` | `count * 4` | ARK sizes as 32-bit integers | Values match actual files exactly |
| variable | 4 | ARK-name array count | Same value as ARK count in PC sample |
| variable | repeated | Length-prefixed ARK names | Includes strings such as `main_pc_0.ark` |
| variable | 4 | Archive tag-set count | Matches ARK count in all three samples |
| variable | repeated | Per-ARK string tag lists | Examples include `launch`, `songs`, and `unordered` |
| variable | 4 | File-record count | 30,382 PC; 35,337 PS4; 39,672 Xbox One |
| variable | repeated | File records | Layout described below |
| variable | 4 | Path-hash bucket count | Same value as file-record count |
| variable | `count * 4` | Path-hash bucket heads | Record index or `0xffffffff` sentinel |

## File-record layout

The serialization routine writes each record in this order:

| Relative order | Size | Meaning |
|---:|---:|---|
| 0 | 8 | Global offset in the concatenated declared ARK space |
| 1 | variable | Length-prefixed internal path |
| 2 | 4 | Previous record index in the same path-hash collision chain, or `0xffffffff` |
| 3 | 4 | File size in bytes |

## Path-hash index

The game hashes each path with MurmurHash3 x64 128 using the 32-bit seed
`0x9520e3c4`. The low 64-bit result modulo the file-record count selects a
bucket. Records are inserted in serialization order: the bucket's former head
is stored in the record's collision-chain field, then the bucket head becomes
the current record index.

ForgeInspector reconstructs this index and requires every chain link and final
bucket head to match. This validates all 105,391 paths across the PC, PS4, and
Xbox One samples rather than merely checking that the stored indices are in
range. The hash routine was identified in `rbvr.exe` at VA `0x140C118A0`; its
caller at VA `0x140C0F360` supplies the seed and performs the modulo operation.

The offset and size interpretation was checked across all 105,391 records from
the three samples. Every offset is within the sum of declared ARK sizes, and
every `offset + size` span remains in that range. All zero-size records are
marker records in the current samples:

| Set | Records | Zero-size records | Marker records |
|---|---:|---:|---:|
| PC | 30,382 | 4 | 4 |
| PS4 | 35,337 | 204 | 204 |
| Xbox One | 39,672 | 289 | 289 |

Console marker offsets at the transition to patch content also equal the total
size of preceding main volumes, providing an independent check of the global
concatenated-offset interpretation.

## Mapping global offsets to ARK volumes

Treat the declared ARK files as one concatenated address space in their header
order. The start of a volume is the sum of all preceding declared sizes. A
record belongs to the volume whose half-open range contains its global offset;
its local offset is `global offset - volume start`.

ForgeInspector v0.3.0 accepts a mapping only when the record's complete byte
range remains within that volume. A zero-size marker at the final end of the
concatenated space maps to the end of the final volume. This rule avoids
silently treating a malformed cross-volume span as readable data.

Exact-path lookup follows the serialized hash index instead of scanning the
record table. The current read operation is deliberately bounded to the first
32 bytes of the matched record, which is sufficient for initial payload-format
identification without creating extracted copies of proprietary assets.

## Cross-platform validation

| Set | Decoded version | Decoded ARK count | Files on disk |
|---|---:|---:|---|
| Rock Band VR PC | 10 | 5 | 5 `main_pc_*` volumes |
| Rock Band 4 PS4 | 10 | 62 | 57 `main_ps4_*` + 5 `patch_main_ps4_*` volumes |
| Rock Band 4 Xbox One | 10 | 58 | 50 `main_xb1_*` + 8 `patch_main_xb1_*` volumes |

The existing ForgeInspector discovery initially reported only `main_*` files.
The decoded counts demonstrate that the patch volumes also belong to each
console header's archive list.

## File-size validation results

ForgeInspector v0.2.0 compares each decoded 32-bit size with the corresponding
local file without changing it.

- PS4: all 62 declared files are present and match.
- Xbox One: all 58 declared files are present and match.
- Rock Band VR PC: all 5 files are present; 4 match.
- `main_pc_1.ark` declares 1,784,806,831 bytes but the local file contains
  1,784,806,891 bytes, a 60-byte excess.

The final 60 local bytes were inspected only to characterize the discrepancy;
they are not all zero. Their purpose and origin are unknown, so the parser does
not silently ignore or remove them.

## Remaining unknowns

- Meaning and byte order of the 16-byte value at `0x08`
- Meaning of the value at `0x04`
- Whether all version-10 headers use the same optional sections

