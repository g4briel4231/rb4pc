# Runtime UI primitives

Version 0.16.0 adds clipped rectangle drawing on the software framebuffer and a
platform-independent menu model with deterministic wraparound navigation. These
are foundational primitives, not a recreation of Rock Band's interface.
## Runtime text

Version 0.80.0 adds an original 5x7 bitmap font for the ASCII letters, digits,
and punctuation needed by the runtime. Text is rendered directly into the
software framebuffer with integer scaling and clipping through the existing UI
primitives. The song browser uses it for its title, entry names, invalid-state
labels, and input hints.
