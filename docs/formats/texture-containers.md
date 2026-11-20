# Texture containers

Version 0.57.0 adds read-only inspection of Forge texture containers. The
research inputs remain outside this repository and must come from a user's own
legally obtained game data.

## Verified paired observation

One matched 512x512 texture extracted from PS4 and PC/RBVR archives was parsed
using the layout implemented by LibForge's public `TextureReader`:

| Field | PS4 | PC |
|---|---:|---:|
| outer magic | 6 | 6 |
| platform version | `0x0c` | `0x09` |
| opaque header bytes | 124 | 172 |
| mipmap entry bytes | 20 | 12 |
| marker after entries | 7 | 6 |
| mipmaps | 8 | 8 |
| footer bytes | 28 | 28 |

Every paired mipmap had the same dimensions, flags, byte length, and SHA-256.
The payloads were therefore byte-identical for this sample; only container
metadata differed. This is evidence that this texture can preserve compressed
payload data during a future container conversion. It is not yet evidence that
all textures use the same opaque header fields.

Files exported by ForgeTool with names ending in `.png_ps4` or `.png_pc` may be
ordinary PNG images beginning with `89 50 4e 47`. Such an export is not a Forge
texture container and is intentionally rejected by `rb4pc-texture-probe`.

## Reproduction

```text
rb4pc-texture-probe <ps4-container> <pc-reference-container>
```

The command parses both files, hashes every mipmap independently, and reports
whether dimensions, flags, lengths, and bytes match. It performs no writes and
does not claim that renaming or copying a PNG creates a valid PC container.

## Conversion boundary

LibForge's `TextureWriter` serializes an already-populated `Texture`; it does
not translate the PS4 opaque header to the PC header. RB4PC will not emit a
`.png_pc` until the PC header fields are understood across multiple paired
samples and the result can be verified by an independent reader/runtime.

## v0.58 corpus census

The recursive census was run against 3,017 user-supplied files carrying the
`.png_ps4` extension. The extension did not reliably identify the container:

| Signature | Parsed | Observation |
|---|---:|---|
| magic 6, version `0x0c` | 561 | PS4-style marker 7 |
| magic 6, version `0x09` | 304 | PC-style marker 6 despite `_ps4` suffix |
| magic 6, version `0x08` | 131 | older marker-6 variant |
| magic 4, version `0x08` | 837 | observed 164-byte opaque header |
| magic 3, version `0x07` | 1,033 | unsupported; not parsed |
| magic 3, version `0x08` | 151 | unsupported; not parsed |

The 1,833 accepted files form eleven structural families, including 256x256
payloads stored at both 4 and 8 bits per pixel and one 512x512 example in each
of two container families. The public LibForge source uses a 168-byte header
for magic 4; the real magic-4/version-8 corpus instead places the mipmap count
after 164 opaque bytes. RB4PC handles only that explicitly observed combination
and does not extrapolate the difference to other versions.

These results also establish that renaming based on `_ps4`/`_pc` is unreliable.
A future converter must inspect the binary signature and structural family.
