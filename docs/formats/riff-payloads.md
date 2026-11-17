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
