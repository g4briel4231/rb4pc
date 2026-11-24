# Native PC audio audit

Version 0.150.0 validates audio structures in place inside PC HDR/ARK sets.

```text
rb4pc-builder <hdr-or-directory> --catalog-pc-audio <catalog.json>
```

The operation reads bounded headers and signatures only. It does not extract,
decrypt, rewrite, or play the source records.

## Checks

For `.mogg` records, the audit reads the little-endian version and Ogg offset,
requires version 10 for the currently supported family, bounds-checks the
offset, and verifies `OggS` at that location.

For `.wav`, `.bank`, and `.fev` records, it verifies the RIFF signature and
classifies the top-level form as `WAVE` or `FEV `. Other catalogued audio
families remain explicitly opaque.

The JSON contains header totals, audio counts and bytes, MOGG versions, embedded
Ogg results, RIFF form counts, unreadable records, invalid-reason counts, and
bounded HDR diagnostics.

## Verified corpus result

The user-owned local Rock Band VR corpus produced:

| Measurement | Result |
| --- | ---: |
| HDR sets | 23 valid / 23 candidates |
| Audio records | 4,806 |
| Audio bytes | 3,981,520,152 |
| MOGG records | 1,738 |
| MOGG v10 with embedded Ogg | 1,640 |
| Unsupported or potentially protected MOGG headers | 98 |
| `.wav` records | 3,054 |
| RIFF/WAVE forms | 3,051 |
| Bank records / RIFF/FEV forms | 13 / 13 |
| Opaque audio records | 1 |
| Unreadable records | 0 |

The 98 unrecognized MOGG headers are not labeled encrypted without further
evidence. They are grouped as unsupported or potentially protected instead of
being misreported as 98 random format versions. Likewise, the three `.wav`
records without RIFF remain explicit exceptions.

## Compatibility boundary

This audit proves structural recognition, not full playback compatibility.
RB4PC already decodes the supported version-10 embedded-Ogg family and PCM
WAVE. Future research is required for the 98 protected/unsupported MOGGs, the
three non-RIFF `.wav` records, FMOD event semantics, and the opaque noise asset.
