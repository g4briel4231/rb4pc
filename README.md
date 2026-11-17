# RB4PC

RB4PC is an experimental, clean-room effort to build independent PC tooling and
runtime components compatible with assets supplied by users from their own
legally obtained copy of Rock Band 4.

The repository does **not** contain game executables, songs, ARK/HDR archives,
console libraries, or other proprietary game content. Do not add those files to
the repository.

## Current milestone: v0.1.0 Foundation

The first executable is `rb4pc-forge-inspector`, a read-only tool for recording
verifiable properties of candidate files. It currently reports the file size and
the first 32 bytes in hexadecimal. Format parsing will only be added as findings
are tested and documented.

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
./build/default/rb4pc-forge-inspector.exe "../path/to/main_ps4.hdr"
```

## Repository layout

- `include/rb4pc`: public C++ interfaces
- `src/inspector`: ForgeInspector implementation and command-line application
- `tests`: automated tests using synthetic, non-proprietary data
- `docs/formats`: evidence-backed format notes (planned)

## Legal notice

RB4PC is an independent community project and is not affiliated with or endorsed
by Harmonix, Epic Games, Electronic Arts, Sony, or Microsoft. Users are
responsible for complying with applicable laws and licenses. No ownership of the
Rock Band trademarks or original game content is claimed.

