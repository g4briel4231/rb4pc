# Music payload census

ForgeInspector v0.11.0 sampled the first 32 bytes of music-related records.
None expose standard `MThd` MIDI or `OggS` page headers at byte zero, so direct
MIDI/Ogg parsing would be unjustified without decoding their outer containers.

Rock Band VR PC contains 5,964 `.mid_pc`, 5,927 `.midisong_pc`, 1,716 `.mogg`,
and 76 `.rbmid_pc` records. The PS4 and Xbox One sets each expose 120 `.mogg`
records under the currently targeted extensions; their counts, total bytes,
size ranges, and five distinct four-byte prefixes match each other exactly.

The high number of distinct prefixes in compiled PC MIDI records indicates that
the first word is not a single stable magic value. This milestone records the
observation only; encryption, compression, or serialization semantics have not
been assigned.

## Forge custom-song invariant

Version 0.21.0 validated all 22 installed ForgeToolGUI packages. Every package
contains nonempty `.mid_pc` and `.midisong_pc` sets, exactly one `.rbmid_pc`,
exactly one `.mogg`, and at least one `.songdta_pc` plus one
`.moggsong_dta_pc`. This establishes package completeness without decoding any
of those formats.

Custom-song assets consistently use `pc/songs_download/<shortname>/`. The
observed `.songdta_pc` and `.moggsong_dta_pc` payloads do not begin as readable
DTA text, so v0.22.0 treats them as opaque compiled payloads.

## Standard MIDI reference

Version 0.24.0 parsed the known source `notes.mid` corresponding to the
installed `angel3` custom-song package. The 76,571-byte format-1 file contains
six tracks at 480 ticks per quarter note and 218 tempo events. Named gameplay
tracks are `EVENTS`, `PART GUITAR`, `PART BASS`, `PART DRUMS`, and
`PART VOCALS`; their respective note-on counts are 0, 4,556, 2,939, 3,067,
and 497. The unnamed tempo track ends at tick 273,600.

These measurements describe the user-supplied standard-MIDI input only. They do
not establish the serialization, compression, or encryption used by the
installed `angel3.rbmid_pc` payload.
