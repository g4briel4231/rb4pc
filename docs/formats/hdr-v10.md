# Forge HDR version 10: verified prefix

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

The name-array location for the larger console samples follows the size array;
full structural parsing has not yet been implemented in RB4PC.

## Cross-platform validation

| Set | Decoded version | Decoded ARK count | Files on disk |
|---|---:|---:|---|
| Rock Band VR PC | 10 | 5 | 5 `main_pc_*` volumes |
| Rock Band 4 PS4 | 10 | 62 | 57 `main_ps4_*` + 5 `patch_main_ps4_*` volumes |
| Rock Band 4 Xbox One | 10 | 58 | 50 `main_xb1_*` + 8 `patch_main_xb1_*` volumes |

The existing ForgeInspector discovery initially reported only `main_*` files.
The decoded counts demonstrate that the patch volumes also belong to each
console header's archive list.

## Remaining unknowns

- Meaning and byte order of the 16-byte value at `0x08`
- Meaning of the value at `0x04`
- Fields following the archive-name array
- File-record layout, offsets, path table, and hash/index structures
- Whether all version-10 headers use the same optional sections

