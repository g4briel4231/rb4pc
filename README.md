# RB4PC

RB4PC is an experimental, clean-room effort to build independent PC tooling and
runtime components compatible with assets supplied by users from their own
legally obtained copy of Rock Band 4.

The repository does **not** contain game executables, songs, ARK/HDR archives,
console libraries, or other proprietary game content. Do not add those files to
the repository.

## Current milestone: v0.4.0 Internal Format Analysis

The first executable is `rb4pc-forge-inspector`, a read-only tool for recording
verifiable properties of candidate files. It reports the file size, SHA-256,
first 32 bytes, a filename-derived platform hint, and the ordered ARK volumes
associated with an HDR file. Format parsing will only be added as findings are
tested and documented.

The report also records Shannon entropy globally and per 64 KiB block, zero-byte
counts, and exactly repeated full blocks. These measurements describe byte
distributions; they do not by themselves prove compression or encryption.

Version 10 HDR files are decoded with the verified Park-Miller XOR stream. The
report lists every ARK declared by the header—including patch volumes—and checks
each declared size against the local file without modifying game data.

The decoder also reads per-archive tags and the complete internal file-record
table. Version 0.2.2 also decodes and validates the final path-hash bucket table
and each record's collision-chain link.

Version 0.3.0 begins read-only ARK access by translating each record's global
offset into a declared volume index and a local offset. A record is resolved
only when its complete byte range stays inside one volume; cross-volume and
out-of-range spans are rejected.

Version 0.3.1 adds the optional payload catalog. It samples at most 16 bytes
from each non-empty record, groups internal filename extensions, recognizes a
small set of established magic values, and retains representative paths for
each signature group. Short zlib and gzip matches are labeled as candidates
rather than treated as proof of compression.

## Build

Requirements:

- CMake 3.25 or newer
- A C++20 compiler
- Ninja when using the included preset

```powershell
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Run the inspector against a file stored outside this repository:

```powershell
./build/default/rb4pc-forge-inspector.exe "../path/to/main_ps4.hdr" --output report-ps4.json
```

Look up an exact internal path through the HDR hash index and read a bounded
32-byte preview directly from its ARK volume:

```powershell
./build/default/rb4pc-forge-inspector.exe "../path/to/main_ps4.hdr" --find "path/inside/archive"
```

This operation is read-only. It does not extract the complete record or alter
the user's HDR/ARK files.

Catalog extensions and recognized payload signatures using a maximum 16-byte
sample from each non-empty record:

```powershell
./build/default/rb4pc-forge-inspector.exe "../path/to/main_ps4.hdr" --catalog
```

Unknown signatures remain explicitly labeled `unknown`; the tool does not infer
compression or a file type solely from an internal filename extension.

Extract one exact internal path to an explicit new destination:

```powershell
./build/default/rb4pc-forge-inspector.exe "../path/to/main_ps4.hdr" `
  --extract "path/inside/archive" --destination "./output.bin"
```

The extractor refuses to overwrite an existing destination, copies through a
temporary file, validates the record against the actual ARK size, and reports
the extracted size and SHA-256. The internal path is never used as a host
filesystem destination.

Extract a filtered set while preserving its safe internal hierarchy:

```powershell
./build/default/rb4pc-forge-inspector.exe "../path/to/main_ps4.hdr" `
  --extract-prefix "songs/example/" --extract-extension ".mogg" `
  --destination-dir "./selected-assets"
```

Prefix and extension filters may be used separately or together. Zero-size
markers are excluded. The destination directory must not already exist, and
the completed directory contains `rb4pc-manifest.json` with each internal path,
relative output path, size, and SHA-256. Absolute paths, drive-qualified names,
empty components, `.` and `..` components are rejected.

Version 0.4.0 begins structural parsing of payload formats with RIFF. During
`--catalog`, every RIFF record's logical size is compared with its HDR record
size and its top-level chunk table is walked with alignment and boundary
validation. Reports include valid/invalid counts, form types, chunk identifiers,
and whether any trailing allocation region is zero or nonzero.

Reports use a versioned JSON schema so future research remains reproducible.
The platform hint is based only on the filename suffix and is not presented as
evidence about the file's internal format.

## Repository layout

- `include/rb4pc`: public C++ interfaces
- `src/inspector`: ForgeInspector implementation and command-line application
- `tests`: automated tests using synthetic, non-proprietary data
- `docs/formats`: evidence-backed format notes (planned)
- `docs/research`: reproducible static-analysis notes without proprietary samples

## Legal notice

RB4PC is an independent community project and is not affiliated with or endorsed
by Harmonix, Epic Games, Electronic Arts, Sony, or Microsoft. Users are
responsible for complying with applicable laws and licenses. No ownership of the
Rock Band trademarks or original game content is claimed.

