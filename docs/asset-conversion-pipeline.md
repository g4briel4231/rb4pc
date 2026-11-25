# Verified asset conversion pipeline

Version 0.2000.0 combines the verified texture writer and embedded-Ogg extractor
without expanding either compatibility boundary.

```text
rb4pc-builder <asset-root> --convert-asset-pipeline <new-output-directory>
```

The destination must not exist. RB4PC plans both asset families, converts only
supported textures, extracts only MOGGs with a verified embedded `OggS`
signature, and preserves relative paths beneath `textures/` and `audio/`.
Per-file manifests and batch summaries remain beside their outputs. The root
`asset-pipeline-summary.json` records the converted, extracted, and skipped
counts.

The entire output is assembled in a temporary sibling tree. Any failure removes
that tree; one final rename publishes the completed result. Native Ogg/WAVE,
read-only textures, research-only audio, and invalid inputs are reported but not
copied or rewritten.