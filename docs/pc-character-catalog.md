# PC character and animation inventory

Version 0.160.0 measures native character/animation families in place:

```text
rb4pc-builder <hdr-or-directory> --catalog-pc-characters <catalog.json>
```

The command is read-only and performs bounded four-byte reads from matching
records. It does not extract, deserialize, convert, or render character assets.

## Families and measurements

For `charanim`, `charclip`, `char`, `skel`, `bonemask`, and `stance`, schema
version 1 reports:

- record and byte totals;
- minimum and maximum record sizes;
- first-four-byte distributions;
- the same prefix interpreted as an unsigned little-endian 32-bit value;
- empty and unreadable totals;
- bounded diagnostics for invalid HDR sets.

## Verified corpus result

The user-owned local Rock Band VR corpus produced 3,484 records and 671,475,650
bytes, with zero empty and zero unreadable records.

| Family | Records | Bytes | Min | Max | Observed first u32 |
| --- | ---: | ---: | ---: | ---: | ---: |
| `charanim` | 1,682 | 212,138,956 | 917 | 969,239 | 17 (`0x11`) |
| `charclip` | 1,405 | 428,255,335 | 6,087 | 2,956,559 | 1 (`0x01`) |
| `char` | 198 | 29,442,168 | 1,044 | 6,267,404 | 13 (`0x0d`) |
| `skel` | 105 | 969,093 | 153 | 22,806 | 1 (`0x01`) |
| `bonemask` | 81 | 427,320 | 536 | 7,104 | 17 (`0x11`) |
| `stance` | 13 | 242,778 | 5,313 | 68,591 | 0 (`0x00`) |

Each family had exactly one prefix value across the corpus. The correlation is
strong evidence of a stable family discriminator or version field, but the
semantic name remains unproven until internal structures or an independent
implementation corroborate it.

## Next boundary

The next step is bounded structural decoding per family: length fields, object
references, skeleton/bone tables, clip timing, and animation channels. Writers
or converters should only be added after those fields are independently
validated on multiple records.
