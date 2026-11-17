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

