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

### 5. Analyze a texture folder

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
manifests and `batch-summary.json`.
### 6. Generate an asset catalog

Open **Tools > PC Asset Catalog**, then choose one `*_pc.hdr` file or a folder
and select a destination for the JSON report.

The report summarizes archives, namespaces, extensions, asset categories, and
known native, intermediate, or unknown path families without extracting assets.

### 7. Generate an audio catalog

Open **Tools > PC Audio Catalog**, select a PC HDR or folder, and choose a JSON
destination.

The report identifies supported MOGG/Ogg, RIFF/WAVE, RIFF/FEV, opaque,
unreadable, and unsupported or potentially protected audio records. Recognition
does not mean that every audio variant can already be played or converted.

### 8. Generate a character and animation catalog

Open **Tools > Character/Animation Inventory**, select a PC HDR or folder, and
choose a JSON destination.

The catalog reports aggregate structural measurements for character definitions,
animations, clips, skeletons, bone masks, and stances. Candidate fields are
measurements, not confirmed semantic definitions.

### 9. Generate a metadata catalog

Open **Tools > PC Metadata Catalog**, select a PC HDR or folder, and choose a
JSON destination.

The catalog covers `.rbsong`, `.songdta_pc`, `.moggsong_dta_pc`, `.csv_pc`, and
`.dta` records. It reports privacy-preserving HDR cohort coverage, aligned 32-bit
profiles, evidence classes, and bounded-offset candidates without recording
proprietary strings or internal paths.

A successful result shows that every profiled field received exactly one
structural evidence class.

### 10. Run the complete PC corpus audit

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

### 11. Read warnings correctly

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
