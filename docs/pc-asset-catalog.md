# Native PC asset catalog

Version 0.130.0 turns the PC archive research into a reproducible inventory.

```text
rb4pc-builder <hdr-or-directory> --catalog-pc-assets <catalog.json>
```

The source may be one HDR or a directory recursively containing HDR files. The
operation is read-only. The destination must not exist.

## Schema

Schema version 1 reports:

- candidate, valid, and invalid HDR counts;
- declared ARK volume, record, and payload-byte totals;
- marker-record count;
- native, intermediate, and unknown path-family counts;
- normalized extension families;
- top-level PC namespaces;
- asset categories;
- bounded diagnostics for rejected HDRs;
- unknown extensions as an explicit follow-up queue.

`nativePathFamily` means the internal filename belongs to an observed native PC
family. It does not mean RB4PC understands or can recreate the payload bytes.
Format compatibility requires a separate structural parser and validation.

## Categories

The current classifier recognizes:

- `chart`: compiled MIDI, MIDI-song, and RBMID;
- `audio`: MOGG, PCM WAVE, FMOD banks, FEV, and noise data;
- `scene-rendering`: entities, scenes, graphs, Fusion data, shaders, and FBX meshes;
- `texture`: PNG/BMP containers and texture arrays;
- `character-animation`: characters, clips, animations, skeletons, masks, and stances;
- `metadata`: DTA-derived records, song data, RBSong, and CSV;
- `lipsync`, `script`, `performance-pattern`, `ui`, `video`, and `marker`.

FBX mesh paths embed an object name after `.fbx++mesh++`. The catalog
normalizes all of them to `.fbx++mesh++*_pc`, preventing object names from
inflating the extension count.

## Verified corpus result

The user-owned local Rock Band VR corpus produced:

| Measurement | Result |
| --- | ---: |
| HDR sets | 23 valid / 23 candidates |
| ARK volumes | 27 |
| Records | 35,582 |
| Declared payload bytes | 8,338,130,919 |
| Path families | 35,582 native / 0 intermediate / 0 unknown |

Category counts:

| Category | Records |
| --- | ---: |
| chart | 15,235 |
| audio | 4,806 |
| scene-rendering | 3,865 |
| texture | 3,647 |
| character-animation | 3,484 |
| metadata | 3,122 |
| lipsync | 1,033 |
| script | 256 |
| performance-pattern | 102 |
| UI | 26 |
| marker | 4 |
| video | 2 |

No proprietary path listing or payload is committed. Only aggregate findings
and the clean-room classifier are stored in the repository.

## Next boundary

This inventory identifies the work required for the port. Later milestones
must add independent parsers and generators per category, beginning with native
song metadata and RBMID, then textures, character assets, scenes, and scripts.
