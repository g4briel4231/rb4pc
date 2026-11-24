# PC character and animation structural profile

Version 0.170.0 extends the native character/animation inventory with a bounded structural profile:

```text
rb4pc-builder <hdr-or-directory> --catalog-pc-characters <catalog.json>
```

The command is read-only and performs bounded 64-byte reads from matching
records. It does not extract, deserialize, convert, or render character assets.

## Families and measurements

For `charanim`, `charclip`, `char`, `skel`, `bonemask`, and `stance`, schema
version 2 reports:

- record and byte totals;
- minimum and maximum record sizes;
- first-four-byte distributions;
- the same prefix interpreted as an unsigned little-endian 32-bit value;
- empty and unreadable totals;
- bounded diagnostics for invalid HDR sets.

## Schema version 2 structural fields

For every complete little-endian 32-bit word in the first 64 bytes, the catalog
reports its byte offset, observation count, distinct-value count, zero count,
minimum, maximum, and how often it is within the record size, equals the full
record size, or equals the bytes remaining after that word. These are candidate
signals for lengths, versions, flags, and references; they are deliberately not
assigned semantics without independent corroboration.

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

The next step is to correlate stable candidate fields with object references,
skeleton/bone tables, clip timing, and animation channels. Writers or
converters should only be added after those fields are independently
validated on multiple records.
