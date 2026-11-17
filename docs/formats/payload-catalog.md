# Payload catalog observations

ForgeInspector v0.3.1 reads at most 16 bytes from each non-empty HDR record.
It does not extract complete assets. Classification is based on byte signatures;
extensions are counted independently and are not used to infer a format.

## Initial results

| Set | Total records | Non-empty analyzed | Empty | Unreadable | RIFF | Unknown |
|---|---:|---:|---:|---:|---:|---:|
| Rock Band VR PC | 30,382 | 30,378 | 4 | 0 | 3,064 | 27,311 |
| Rock Band 4 PS4 | 35,337 | 35,133 | 204 | 0 | 1,016 | 34,117 |
| Rock Band 4 Xbox One | 39,672 | 39,383 | 289 | 0 | 1,016 | 38,367 |

The PC set also has two zlib-header candidates and one gzip-header candidate.
These two-byte matches are deliberately not presented as confirmed compressed
streams; random or format-specific fields can produce the same prefix. All
three occur in compiled `.mid_pc` or `.midisong_pc` records, reinforcing the
need to inspect those container formats before assigning compression semantics.

The first RIFF examples in the PC set are `.bank` files under
`pc/fmod_banks/desktop`, which is consistent with an audio-container signature.

The large unknown groups are expected to contain platform-specific Forge and
game formats such as `.mid_pc`, `.bmp_pc`, `.charanim_pc`, and their console
counterparts. Identifying those formats requires evidence beyond an extension
or a short prefix.

## Reproduction

```powershell
rb4pc-forge-inspector main_pc.hdr --catalog
rb4pc-forge-inspector main_ps4.hdr --catalog
rb4pc-forge-inspector main_xb1.hdr --catalog
```
