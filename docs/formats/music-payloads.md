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
