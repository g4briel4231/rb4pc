<p align="center">
  <img src="assets/branding/rb4pc-logo2.png" alt="RB4PC Builder" width="760">
</p>

# RB4PC Builder

RB4PC Builder is an experimental clean-room project for developing independent
PC tools and runtime components compatible with assets supplied by users from
their own legally obtained copy of Rock Band 4.

The project does **not** include or distribute game executables, songs, console
libraries, original HDR/ARK archives, encryption keys, or other proprietary
game content. RB4PC Builder is not yet a complete Rock Band 4 PC port.

<p align="center">
  <strong><a href="https://github.com/rb4pc/rb4pc-builder/wiki">Open the RB4PC Builder Wiki</a></strong>
</p>

## Using the Builder GUI

### 1. Open the Builder

Run `rb4pc-builder-gui.exe`. The main window provides buttons for opening PC
files and folders and a **Tools** menu containing the available catalogs and
audits.

Current GUI inspection workflows accept PC-named ForgeTool output, validated
`*_pc.hdr` files, and folders containing PC HDR/ARK sets. Inputs named `_ps4`
are intentionally rejected because general direct PS4 conversion is not yet
supported.

### 2. Open a PC file

Select **Open File** and choose a supported PC file. When an HDR is selected,
the Builder verifies its PC filename, version-10 structure, path-hash index,
declared ARK volumes, and record table. The original file is not modified.

### 3. Open a PC folder

Select **Open Folder** and choose a directory containing PC-format files or
HDR/ARK sets. Catalog tools can scan the selected directory recursively.

Keyboard access is available for the current GUI workflows: `Ctrl+O` opens a PC
HDR, `Ctrl+L` clears the selected source, and the File Viewer supports `Ctrl+F`,
`Enter`, `Ctrl+E`, and `Ctrl+C` for search, preview, extraction, and copying the
selected internal path. See the Wiki for the complete guide.

Keep original game data in a separate, backed-up folder. Save reports and other
output to a different location whenever possible.

### 4. Convert a supported PS4 texture

Choose **Tools > Convert Supported PS4 Texture…**. The current write path accepts
only the verified linear magic-6/version-12 family with a 256×256 base image,
seven square mipmaps, flag 1, and consistent 4bpp or 8bpp storage. It writes a
new PC container plus a JSON manifest, preserves the source, and parses and
hash-verifies the output before reporting success. Other texture families are
rejected without creating an output.

### 5. Compare a PS4/PC texture pair

Generate a reproducible JSON comparison without modifying either input:

```text
rb4pc-builder <source-texture> --compare-texture-pair <reference-texture> <new-report.json>
```

The report records container versions, mipmap counts, payload hashes, and the
exact compatibility result.

Compare complete extracted source/reference trees with `rb4pc-builder <source-root>
--compare-texture-directories <reference-root> <new-report.json>`. Matching,
incompatible, and unmatched files are recorded separately.

### 6. Analyze a texture folder

Choose **Tools > Analyze Texture Conversion Folder…** to create a read-only JSON
plan. It scans `.png_ps4` and `.texture` candidates recursively and classifies
each as `convertible`, `readOnly`, or `invalid`, including its measured binary
family and a reason. No asset is modified. The same operation is available as:

```text
rb4pc-builder <texture-directory> --plan-texture-conversion <new-plan.json>
```

After reviewing the plan, the CLI can transactionally convert only supported
entries into a new output directory:

```text
rb4pc-builder <texture-directory> --convert-texture-batch <new-output-directory>
```

The output is first built under a temporary sibling directory. Any failure
removes that temporary tree; success publishes it atomically with per-texture
manifests and `batch-summary.json`. Verify a published batch independently with:

```text
rb4pc-builder <texture-directory> --verify-texture-batch <batch-directory> <new-report.json>
```

Verification reopens source and destination containers, recomputes hashes,
compares every mipmap payload, requires each manifest, and reports missing,
invalid, or unexpected files.

The same operations are available from **Tools > Convert Supported Texture Batch…**
and **Tools > Verify Converted Texture Batch…**. The GUI shows the plan counts and
final destination before conversion begins.
### 7. Build the verified asset pipeline

Choose **Tools > Build Verified Asset Pipeline…** or run:

```text
rb4pc-builder <asset-root> --convert-asset-pipeline <new-output-directory>
```

The transactional output contains `textures/`, `audio/`, individual manifests,
batch summaries, and `asset-pipeline-summary.json`. Only verified families are
written; all other assets remain untouched and are counted as skipped.

### 8. Generate an asset catalog

Open **Tools > PC Asset Catalog**, then choose one `*_pc.hdr` file or a folder
and select a destination for the JSON report.

The report summarizes archives, namespaces, extensions, asset categories, and
known native, intermediate, or unknown path families without extracting assets.

### 9. Plan extracted audio conversion

Choose **Tools > Analyze Audio Conversion Folder…** or run
`rb4pc-builder <audio-root> --plan-audio-conversion <new-report.json>`. The
read-only report separates runtime-ready Ogg/WAVE, extraction-ready embedded-Ogg
MOGG, research-only signatures, and invalid files. It performs no conversion.

Extract one verified embedded stream with `rb4pc-builder <source.mogg>
--extract-mogg-ogg <new-output.ogg>. The source remains unchanged and a SHA-256
manifest accompanies the new Ogg file.

### 10. Generate an audio catalog

Open **Tools > PC Audio Catalog**, select a PC HDR or folder, and choose a JSON
destination.

The report identifies supported MOGG/Ogg, RIFF/WAVE, RIFF/FEV, opaque,
unreadable, and unsupported or potentially protected audio records. Recognition
does not mean that every audio variant can already be played or converted.

### 11. Generate a character and animation catalog

Open **Tools > Character/Animation Inventory**, select a PC HDR or folder, and
choose a JSON destination.

The catalog reports aggregate structural measurements for character definitions,
animations, clips, skeletons, bone masks, and stances. Candidate fields are
measurements, not confirmed semantic definitions.

### 11a. Convert and verify `song.ini` metadata

Choose **Tools > Convert song.ini Metadata…** or run:

```text
rb4pc-builder <song.ini> --convert-song-metadata <new-metadata.json>
```

RB4PC writes canonical JSON to a temporary file, rereads and compares every supported field, hashes both files, and publishes a conversion manifest. Existing destinations are never overwritten. A transactional batch is also available:

```text
rb4pc-builder <song-root> --convert-song-metadata-batch <new-output-directory>
```

The first limited native writer encodes the independently reproduced `mogg_path`/`midi_path` structure:

```text
rb4pc-builder <file.mogg> --write-native-moggsong <file.rbmid> <new-output.moggsong_dta_pc>
```

The installed RBVR fixture, ForgeTool output, and RB4PC output were byte-identical. The observed fixed-size `songdta_pc` layout can also be inspected and reproduced byte for byte:

```text
rb4pc-builder <input.songdta_pc> --inspect-native-songdata <report.json>
rb4pc-builder <input.songdta_pc> --roundtrip-native-songdata <new-output.songdta_pc>
```

New-record conversion still needs explicit values for native-only fields; `csv_pc` and `rbsong` remain research work. See [the metadata guide](docs/formats/song-metadata-writing.md), [moggsong evidence](docs/formats/native-moggsong-metadata.md), and [songdata evidence](docs/formats/native-songdata-pc.md).
### 12. Generate a metadata catalog

Open **Tools > PC Metadata Catalog**, select a PC HDR or folder, and choose a
JSON destination.

The catalog covers `.rbsong`, `.songdta_pc`, `.moggsong_dta_pc`, `.csv_pc`, and
`.dta` records. It reports privacy-preserving HDR cohort coverage, aligned 32-bit
profiles, evidence classes, and bounded-offset candidates without recording
proprietary strings or internal paths.

A successful result shows that every profiled field received exactly one
structural evidence class.

### 13. Run the complete PC corpus audit

Open **Tools > Complete PC Corpus Audit**.

1. Choose **Yes** to select one `*_pc.hdr`, or **No** to select a folder.
2. Select an existing parent folder for the output.
3. The Builder creates a new `rb4pc-pc-audit` directory.

The destination must not already exist. The audit publishes five reports:

- `summary.json`
- `assets.json`
- `audio.json`
- `characters.json`
- `metadata.json`

The audit reports completion only when all catalog passes agree on their HDR
counts, no required record is unreadable, and metadata evidence classification
is complete. Publication is transactional and does not overwrite an existing
audit.

### 14. Read warnings correctly

A warning or an `unknown` classification does not automatically mean a file is
damaged. It means the current implementation cannot safely confirm that
structure. Preserve the JSON report when opening an issue, but never attach
copyrighted game files or extracted proprietary payloads.

## Documentation

For installation, formats, version history, project status, legal policy, and
the roadmap, visit the **[RB4PC Builder Wiki](https://github.com/rb4pc/rb4pc-builder/wiki)**.

RB4PC is an independent community project and is not affiliated with or endorsed
by Harmonix, Epic Games, Electronic Arts, Sony, or Microsoft. Rock Band and all
related trademarks belong to their respective owners.
