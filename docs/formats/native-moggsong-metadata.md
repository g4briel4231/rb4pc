# Native PC moggsong metadata evidence

## Verified structure

The supported payload is a binary DTA root containing exactly two entries:

```text
(mogg_path "<filename>.mogg")
(midi_path "<filename>.rbmid")
```

RB4PC accepts filenames only, rejects directory traversal, requires the expected extensions, writes through a temporary file, parses the binary result back, compares both values, and publishes a SHA-256 manifest.

## Independent reproduction

A user-owned Rock Band VR sample was reproduced with ForgeTool 0.1.19 using `dta2b`. The installed sample, ForgeTool output, and RB4PC output were byte-identical at 146 bytes with SHA-256 `beacd4d195d286473d0cd3601834be9339d1ab8074b4d19aa1aabf0be0f5771f`.

This proves the supported two-entry fixture and encoder layout. It does not prove arbitrary binary DTA nodes or the unrelated `songdta_pc` and `rbsong` formats.