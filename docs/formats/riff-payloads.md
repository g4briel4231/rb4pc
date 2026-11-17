# RIFF payload observations

ForgeInspector v0.4.0 parses RIFF records directly from their ARK locations.
It reads the little-endian RIFF size and form type, then walks top-level chunks
using each chunk's declared size and even-byte alignment. No payload is copied
to the repository.

## Cross-platform results

| Set | RIFF records | Structurally valid | Exact HDR size | Nonzero trailing region |
|---|---:|---:|---:|---:|
| Rock Band VR PC | 3,064 | 3,064 | 3,055 | 9 |
| Rock Band 4 PS4 | 1,016 | 1,016 | 1,004 | 12 |
| Rock Band 4 Xbox One | 1,016 | 1,016 | 1,004 | 12 |

Every RIFF chunk table is structurally valid within its own declared RIFF
size. Two form types were observed:

- `WAVE`: 3,051 PC records and 1,005 records on each console.
- `FEV `: 13 PC records and 11 records on each console.

Common top-level chunks include `fmt ` and `data` for `WAVE`, and `FMT `,
`LIST`, `SND `, `elm1`, `minf`, and `regn` in the observed `FEV ` files.

## HDR record size versus RIFF size

Some Fusion drum-kit WAV records have an HDR size of 65,536 bytes while their
embedded RIFF declares a shorter logical size. The trailing region is nonzero,
so it must not be described as zero padding or silently discarded. The parser
reports both sizes and validates the RIFF only within its declared boundary.
The purpose of the remaining allocation bytes is not yet established.

This distinction matters for extraction: v0.3.x preserves the complete HDR
record, while a future format-aware conversion may choose the logical RIFF
boundary only after the trailing data's role is understood.

## WAVE format metadata

ForgeInspector v0.4.1 decoded a standard `fmt ` prefix for every WAVE record.
All 5,061 WAVE files use format tag 1 (integer PCM) and 16 bits per sample.

| Set | Mono | Stereo | 48 kHz | 44.1 kHz | 32 kHz |
|---|---:|---:|---:|---:|---:|
| Rock Band VR PC | 2,773 | 278 | 2,929 | 106 | 16 |
| Rock Band 4 PS4 | 530 | 475 | 883 | 106 | 16 |
| Rock Band 4 Xbox One | 530 | 475 | 883 | 106 | 16 |

No WAVE record had a missing or shorter-than-16-byte `fmt ` chunk.

## FEV format prefix

ForgeInspector v0.4.2 reads the first two little-endian 32-bit values of each
`FEV `/`FMT ` payload without assigning names to them. Every FEV had the prefix:

- Rock Band VR PC: `80, 70` in all 13 records.
- Rock Band 4 PS4: `99, 99` in all 11 records.
- Rock Band 4 Xbox One: `99, 99` in all 11 records.

This is a reproducible platform/version distinction, but the fields' semantics
are not yet established.
