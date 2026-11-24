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

## v0.59 magic-3 payloads

Magic-3 versions 7 and 8 place the mipmap count at offset `0x94`, followed by
12-byte `(width, height, flags)` entries and marker 5. The aggregate texture
storage begins immediately after the marker. Unlike magic 4 and 6, it has no
per-mipmap length prefixes.

Across the observed corpus, aggregate lengths correspond approximately to 4,
8, or 16 stored bits per pixel, with format- and dimension-dependent
alignment overhead. Because that overhead prevents proven per-level boundaries,
RB4PC reports these containers as an opaque aggregate payload. It records and
hashes the entire range from the post-marker offset through the 28-byte footer.
This makes all files censusable and comparable without pretending the GPU
tiling or swizzle layout has been decoded.

With this opaque representation enabled, the same 3,017-file corpus produces
18 structural families with 3,017 successful parses and zero rejected
signatures.

## v0.60 linear PS4-to-PC conversion

The first write path is intentionally restricted to the family for which the
PC container metadata is supported by a large corpus:

- source magic 6, version `0x0c`, marker 7;
- 256x256 base image with seven square mipmaps through 4x4;
- flag 1 on every level;
- a consistent linear storage rate of either 4 or 8 bits per pixel.

The canonical 172-byte PC header is constant across 201 observed 4bpp files and
89 observed 8bpp files. The 4bpp header also matches byte-for-byte the public
LibForge 256x256 header template after separating its embedded version field.
The 8bpp family differs at only four header byte positions and the generated
header hash matches all 89 observed examples.

Conversion changes the platform version to `0x09`, emits the canonical header,
drops the PS4-only eight bytes from each mipmap entry, changes marker 7 to 6,
and copies every length-prefixed mipmap and the 28-byte footer without altering
them. It creates a JSON manifest containing source, destination, and payload
SHA-256 values. The output is parsed again and compared to the source before
the operation succeeds.

Two real conversions, one 4bpp and one 8bpp, were accepted by both the RB4PC
parser and the independent LibForge `TextureReader`. This proves container
readability and payload preservation; it does not yet prove rendering by the
Rock Band VR engine. Magic-3, magic-4, version-8, 512x512, and other families
remain read-only.

## v0.61 artwork-aware song assembly

The builder can use the verified linear conversion while assembling a song
directory. `--assemble-song-with-art` accepts the three validated core song
files plus a supported `.png_ps4` source and writes the conventional layout:

```text
songs/<shortname>/notes.mid
songs/<shortname>/<shortname>.mogg
songs/<shortname>/<shortname>.mogg.dta
songs/<shortname>/<shortname>.png_pc
songs/<shortname>/<shortname>.png_pc.rb4pc-conversion.json
songs/<shortname>/session.json
```

Artwork assemblies use session schema version 2. The runtime requires the
canonical artwork filenames and verifies the SHA-256 of both the converted
texture and its conversion manifest. A missing, malformed, renamed, or modified
artwork artifact rejects the directory before it can be used by the runtime.

## v0.62 runtime artwork decoding

The runtime decodes the first mipmap of the supported PC texture family as BC1
for 4bpp payloads and BC3 for 8bpp payloads. Decoded pixels use the runtime's
32-bit framebuffer format and can be scaled into the song browser. Automated
fixtures cover both compression modes with a known opaque-red result.
