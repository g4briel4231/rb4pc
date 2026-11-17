# Logical RIFF conversion

ForgeInspector v0.5.0 provides the first format-aware asset conversion. It
accepts one exact internal path, validates the complete RIFF chunk structure,
and copies the logical range declared by the RIFF header to a new host file.

The conversion does not decode or re-encode audio. For exact-size records, the
output is a byte-for-byte copy of the validated RIFF. When the HDR record is
larger than the logical RIFF, every remaining byte is preserved in an adjacent
`.rb4pc-trailing.bin` sidecar. Both outputs receive independent SHA-256 hashes.

This policy makes the operation reversible and avoids claiming that the
nonzero trailing regions observed in 33 Fusion WAV records are disposable.
Source HDR and ARK files are never modified, existing destinations are refused,
and temporary partial outputs are removed when conversion fails.
