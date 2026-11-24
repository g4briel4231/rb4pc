# Complete PC corpus audit

Version 0.340.0 extends the existing read-only PC asset, audio, and
character/animation inspectors into one reproducible audit.

```text
rb4pc-builder <hdr-or-directory> --audit-pc-corpus <new-output-directory>
```

The destination must not already exist. RB4PC computes all three catalogs,
writes them to a temporary sibling directory, adds the metadata catalog and `summary.json`, and renames
the completed directory into place. A failed write removes only that temporary
directory and never overwrites an existing audit.

The output contains:

- `summary.json`: schema, completion state, aggregate counts, and file list;
- `assets.json`: namespaces, extensions, categories, and path-family status;
- `audio.json`: MOGG, Ogg, RIFF/WAVE, FEV, opaque, and unreadable counts;
- characters.json: structural fields, candidates, and privacy-preserving ASCII signals;
- metadata.json: native metadata families, signatures, sizes, and bounded ASCII signals.

The operation is also available in the Windows Builder under
`Tools > Complete PC Corpus Audit...`. The GUI accepts one `*_pc.hdr` or a
folder, then creates `rb4pc-pc-audit` inside a selected parent folder.

The audit is read-only with respect to the selected game data. It does not
extract payloads, copy proprietary paths into the summary, convert assets, or
claim that all inventoried formats are implemented by the runtime.