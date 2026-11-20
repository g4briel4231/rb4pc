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
