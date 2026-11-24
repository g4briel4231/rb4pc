# Deterministic PC HDR/ARK generation

Version 0.101.0 introduces the first write path for PC-style archive pairs.

```text
rb4pc-builder <song-directory> --build-pc-archive <output-dir> <shortname>
```

The source directory must already contain a song assembled and validated by
RB4PC. The output directory must exist, and neither destination may exist.

## Generated layout

The command creates:

```text
<shortname>_pc.hdr
<shortname>_pc_0.ark
```

Regular source files are sorted by their internal path, concatenated into the
single ARK volume, and recorded under:

```text
pc/songs_download/<shortname>/<relative-source-path>
```

The version-10 HDR contains the ARK name and size, every 64-bit global offset,
32-bit record size, and the complete MurmurHash3 collision-chain table. Its
payload is encoded with the verified Park–Miller byte stream. A fixed seed and
identifier make the result reproducible: identical source names and bytes
produce identical HDR and ARK SHA-256 hashes.

## Safety and verification

The Builder:

- rejects invalid short names and symbolic links;
- enforces the v10 32-bit record and single-volume size limits;
- refuses existing final or temporary destinations;
- writes temporary files first;
- checks the final ARK byte count;
- decodes the generated HDR with the independent reader;
- validates its archive declaration, records, and path-hash index;
- removes partial outputs after a failure.

Automated tests build the pair twice, require identical hashes, mount the
result through `inspect_header_set`, and require every record to resolve within
the generated ARK. A real external corpus test also generated and reopened a
six-record, 15,324,217-byte archive without storing its payloads in Git.

## Compatibility boundary

This is structural PC HDR/ARK compatibility, not yet complete engine-level
song compatibility. Standard MIDI and RB4PC session manifests remain useful to
the clean-room runtime but are not native compiled Rock Band VR payloads. Future
milestones must map or recreate the required native metadata and chart formats
before these archive pairs can be described as directly loadable by that game.
