# Complete PC corpus audit

Version 0.800.0 consolidates the read-only PC asset, audio,
character/animation, and metadata inspectors into one reproducible audit.

```text
rb4pc-builder <hdr-or-directory> --audit-pc-corpus <new-output-directory>
```

The destination must not already exist. RB4PC computes all four catalogs,
writes them to a temporary sibling directory, adds `summary.json`, and renames
the completed directory into place. A failed write removes only that temporary
directory and never overwrites an existing audit.

The output contains:

- `summary.json`: schema version 10, completion state, cross-catalog HDR-count consistency, aggregate counts, and file list;
- `assets.json`: namespaces, extensions, categories, and path-family status;
- `audio.json`: MOGG, Ogg, RIFF/WAVE, FEV, opaque, and unreadable counts;
- `characters.json`: structural fields, candidates, and privacy-preserving ASCII signals;
- `metadata.json`: native metadata families, signatures, sizes, aligned 32-bit profiles, and bounded ASCII signals.

`complete` is true only when all four passes find at least one valid HDR, report
no invalid HDRs or unreadable records, and agree on both candidate and valid HDR
counts, and every profiled metadata field has exactly one evidence class.
`consistentHeaderCounts`, `metadataEvidenceComplete`, and
`metadataBoundedOffsetRegionsComplete`, and
`metadataSampledTargetsComplete`, and
`metadataSecondHopRegionsComplete` expose those invariants separately.

The operation is also available in the Windows Builder under
`Tools > Complete PC Corpus Audit...`. The GUI accepts one `*_pc.hdr` or a
folder, then creates `rb4pc-pc-audit` inside a selected parent folder. It reports
metadata totals and the consistency result alongside the other record counts.

The audit is read-only with respect to the selected game data. It does not
extract payloads, copy proprietary paths into the summary, convert assets, or
claim that all inventoried formats are implemented by the runtime.