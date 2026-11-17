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

Version 0.25.0 paired every note-on in the reference chart with a matching
note-off on the same channel and key. Guitar produced 4,556 complete notes,
bass 2,939, drums 3,067, and vocals 497, with no unmatched note-offs or
dangling note-ons in any track. This validates the input-side note lifecycle
needed by later instrument and difficulty mapping.

The raw installed `angel3.rbmid_pc` record begins with `e7 81 1e b6`, not the
little-endian RBVR format value `2f 00 00 00` expected by the public AmpForge
[RBMID reader](https://github.com/SoffiMeower/AmpForge/blob/master/LibForge/LibForge/Midi/RBMidReader.cs).
AmpForge 0.1.19 also rejects the extracted record at its first word. The
observation is recorded without naming the unidentified outer layer.

## Five-fret chart mapping

Version 0.26.0 applies only the conventional five-note difficulty ranges to
`PART GUITAR` and `PART BASS`. On the reference chart it produces 410/423/493/612
guitar chords and 287/327/385/467 bass chords for easy/medium/hard/expert,
respectively. Notes beginning on the same tick are represented as one lane-mask
chord; the longest member determines its sustain end. No drum or vocal mapping
is implied by these counts.

## Tempo timeline

Version 0.27.0 decodes all 218 tempo meta-events in the reference chart and
integrates them over its 480-tick quarter-note division. Tick 273,600, the end
of the tempo track, maps to 265.313 seconds. This timeline is calculated from
standard MIDI only and is not yet synchronized to decoded song audio.

Version 0.28.0 projects the expert five-fret charts onto the tempo timeline.
The reference guitar chart spans 5.569–258.682 seconds and the bass chart spans
42.789–240.013 seconds. Runtime window queries include upcoming chords and any
sustain whose end remains inside the look-behind window.

Version 0.29.0 feeds the timed expert charts through the provisional gameplay
cursor at their exact timestamps. Deterministic autoplay records 612/612 guitar
hits for 61,200 points and 467/467 bass hits for 46,700 points, with zero misses.
This validates lifecycle integration only; it does not validate official scoring.

Version 0.30.0 renders the same timed chord model into a generated five-lane
software highway. Automated coverage verifies that the two-chord fixture draws
three gems and three sustain tails inside the requested visibility window. The
renderer depends on no original textures, fonts, or other visual assets.

Version 0.31.0 adds a platform-neutral input mapping layer over raw controller
state. Tests verify simultaneous fret masks, one-shot strum edges, held-button
suppression, release/retrigger behavior, and pause edges. Hardware-specific
guitar identifiers and layouts remain future evidence work.

Version 0.32.0 integrates frame delta, controller mapping, pause state, misses,
hits, scoring, and highway rendering in one session object. The integration
fixture expires its first chord, hits the second at 0.500 seconds, then pauses
for a one-second host frame without advancing gameplay time.

Version 0.33.0 introduces a PCM-frame transport clock. A 48 kHz fixture advances
exactly 24,000 completed frames to 0.500 seconds, drives the integrated session
to the second chord, and remains fixed while paused. This establishes the clock
contract that a streaming audio backend must report.
