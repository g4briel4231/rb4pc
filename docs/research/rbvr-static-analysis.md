# Rock Band VR executable: static triage

This note records reproducible observations about the Rock Band VR executable.
It does not include any part of the executable and does not claim that a string
or imported function proves how the HDR/ARK data is encoded.

## Input identity

- File: `rbvr.exe`
- Size: 19,603,968 bytes
- SHA-256: `ac4d3dcee57fa1599a0b37fec8e2c6cd486440919a49332ded4829f10a78275e`
- PE timestamp: 2017-04-03 23:50:41 (stored timestamp value)

## PE structure

`llvm-readobj` identifies the file as a native 64-bit Windows PE image:

- Format: COFF x86-64
- Subsystem: Windows GUI
- Entry-point RVA: `0x01390B10`
- Image base: `0x140000000`
- Sections: 10
- `.text`: executable and readable
- `.rdata`: readable
- `.data`: readable and writable
- `.dfpc`: executable, readable, and writable

Reproduction command:

```powershell
llvm-readobj --file-headers --sections --coff-imports rbvr.exe
```

## File-access evidence

The import table includes both Win32 and C runtime file APIs:

- `CreateFileA`, `CreateFileW`, `ReadFile`, `GetFileSizeEx`
- `FindFirstFileA`, `FindNextFileA`, `GetFileAttributesA`
- `fopen`, `fread`, `fseek`, `ftell`, `fclose`

Static ASCII-string inspection found the following format strings in `.rdata`:

| File offset | String or nearby diagnostic |
|---:|---|
| `0x00E0AD4F` | `main_%s.hdr` |
| `0x00E0DB9C` | `%s_0.ark` |
| `0x00E0DBB5` | `%s_%d.ark` |
| `0x00E0DBC5` | `%s/%s.hdr` |
| near `0x00E0DBC5` | `Archive`, `NumFiles`, `Guid`, `Arkfiles` |

The same `.rdata` region also contains `using_arks` and platform/configuration
tokens including `HX_PC`, `HX_MSFT`, `HX_DX11`, and `_SHIP`. These observations
support the existence of a native HDR/ARK loader, but do not yet identify its
function boundaries or on-disk structure.

## Compression and cryptography signals

The executable contains identifiable zlib 1.2.5 strings and `inflate`/`deflate`
messages. The nearby data also contains libpng and libcurl diagnostics, so zlib
presence alone does not establish that HDR files use zlib.

The executable imports these CryptoAPI functions:

- `CryptAcquireContextA`
- `CryptGenRandom`
- `CryptCreateHash`, `CryptHashData`, `CryptGetHashParam`
- `CryptDestroyHash`, `CryptReleaseContext`

No CryptoAPI encrypt/decrypt function is present in the static import table.
The executable nevertheless contains Rijndael self-test strings and a reference
to `core/src/wwise_obj/tomcrypt`, showing that some internally linked tomcrypt
code exists. Its relationship to HDR/ARK loading is not established.

## Executable-protection layer

Strings at file offsets around `0x012AC0D8` and `0x012AC5EB` include:

- `Reading from a protected file`
- `Decrypt Key`
- `Hook_ReadFile`
- `Asynchronous read on protected file detected`

These offsets fall inside the `.dfpc` section, whose section flags allow read,
write, and execute access. This is recorded separately from the `.rdata`
HDR/ARK evidence: it is consistent with an executable-protection layer, but it
does not prove that Forge archives use the same mechanism.

## Current conclusions

1. `rbvr.exe` contains a native HDR/ARK path construction and loading subsystem.
2. High entropy in the sampled HDR files remains unexplained.
3. zlib, tomcrypt/Rijndael, CryptoAPI hashing, and a protected-file layer are
   present, but none can yet be attributed to HDR decoding.
4. The next useful step is to identify code references to the four HDR/ARK
   format strings and map the surrounding functions and calls.

## HDR/ARK code references

The following virtual addresses use image base `0x140000000`. They were obtained
from RIP-relative targets printed by `llvm-objdump`.

Corrected string starts and direct references:

| String VA | Text | Direct reference sites found |
|---:|---|---|
| `0x140E0EF50` | `main_%s` | `0x14070717A` |
| `0x140E0EF98` | `%s_0.ark` | `0x140709463` |
| `0x140E0EFB0` | `%s_%d.ark` | `0x140707BDA`, `0x140708CA0`, `0x1407098AA` |
| `0x140E0EFC0` | `%s/%s.hdr` | `0x140709D4F` |

Reproduction examples:

```powershell
llvm-objdump -d rbvr.exe | Select-String '# 0x140e0ef'
llvm-objdump -d --start-address=0x140709380 `
  --stop-address=0x140709f00 rbvr.exe
```

### ARK path helper at `0x140707BC0`

An `int3`-delimited function starts at `0x140707BC0`. It loads the
`%s_%d.ark` format string at `0x140707BDA`, incorporates a string-like field at
object offset `+0xA8`, and incorporates the other incoming argument. The calls
around `0x1405FCB80` through `0x1405FD800` behave like string construction and
formatting operations, but their exact types have not been established.

### Large archive-processing function at `0x140709380`

An `int3`-aligned function begins at `0x140709380`. Observed behavior includes:

1. If an object collection at offsets `+0x68`/`+0x70` is not empty, processing
   continues; otherwise it branches near the function's later exit path.
2. When the ARK-path collection at object offset `+0x28` is empty, the function
   constructs `%s_0.ark` at `0x140709463` using the field at `+0xA8`.
3. Other paths construct `%s_%d.ark` at `0x140708CA0` and `0x1407098AA`.
4. A virtual stream-like object is called repeatedly. A loop beginning near
   `0x140709B70` requests chunks of `0x1000` bytes and passes the returned byte
   count to `0x140607CE0`.
5. The function constructs `%s/%s.hdr` at `0x140709D4F` from an incoming value
   and the object field at `+0xA8`.
6. The resulting path is passed to `0x140608130` with `r8d = 4` and `r9b = 1`.
   Subsequent calls around `0x140607DC0` repeatedly operate on four-byte values.

This establishes that one function coordinates numbered ARK paths, a header
path, stream operations, and metadata-sized values. It is not yet safe to label
the function solely as a parser: the observed operations may include loading,
copying, rebuilding, debugging, or serialization paths.

## Revised next step

Map the callees around `0x140608130`, `0x140607770`, `0x140607CE0`, and
`0x140607DC0`, then identify whether they implement file open, buffered I/O,
byte-order conversion, serialization, or another abstraction. Function names
must remain provisional until supported by call behavior or runtime evidence.

## Callee map and stream transformation

The four callees can now be classified from their behavior:

| Address | Verified behavior |
|---:|---|
| `0x140608130` | Constructs a file-backed stream object from a path, mode value, and flag |
| `0x140607770` | Generates and writes a 32-bit seed, then creates transformation state at object offset `+0x18` |
| `0x140607CE0` | Encodes a buffer in chunks up to `0x200`, then calls virtual method `+0x60` |
| `0x140607DC0` | Optionally byte-swaps a scalar of size 1, 2, 4, or 8 and passes it to `0x140607CE0` |

The complementary function at `0x140607A00` calls virtual method `+0x58` first
and then applies the same XOR transformation in place. This establishes `+0x58`
as the read-side operation and `+0x60` as the write-side operation for this
stream abstraction.

`0x140607690` is the read-side initialization path: it reads a four-byte seed,
optionally byte-swaps it, and initializes state at `+0x18`. `0x140607770` is the
write-side counterpart: the seed is written before state is installed, so the
first four bytes remain unencoded.

The state constructor at `0x140C0F430` normalizes zero to 1 and negates negative
seeds. The next-value function at `0x140C0F450` is the Park-Miller
minimal-standard generator using constants 16807, 127773, 2836, and modulus
2147483647. The low byte of each generated value is XORed with one data byte.

This makes the role of `0x140709380` substantially clearer: it is a header
construction/serialization path, not the primary parsing path. It opens an HDR
stream, writes the seed through `0x140607770`, and writes typed values through
`0x140607DC0`.

Decoding the three available HDR prefixes with this algorithm produced coherent
version, count, size, and filename data. The partial verified layout is recorded
in [`../formats/hdr-v10.md`](../formats/hdr-v10.md).

