# Controller prompts

RB4PC uses Xbox-style controller prompts by default because its initial Windows
input backend is XInput. The prompt model is separate from raw button masks so
the UI can later select another theme without changing gameplay actions.

Version 0.42.0 provides original procedural icons for:

- A, B, X, and Y face buttons;
- the directional pad;
- left/right bumpers and triggers;
- left/right stick actions;
- Menu and View.

The renderer uses framebuffer primitives and a small built-in glyph set. It does
not extract or redistribute controller artwork from Rock Band 4 or another game.
Xbox is the explicit default theme, not a platform guess made by individual
screens.

Semantic UI actions map to the theme centrally. In the Xbox theme, Confirm is A,
Cancel is B, navigation is the D-pad, tab changes use LB/RB, Pause uses Menu,
and the secondary-view action uses View. Screens request an action rather than
hard-coding a lettered button.

## Windows input dependency

The current Windows executable links against the operating system's
`XINPUT1_4.dll`. A dependency-table check of `rb4pc-input-probe.exe` confirms
that exact import. RB4PC must not bundle an arbitrary XInput DLL beside the
executable; supported Windows 10/11 installations provide it as a system
component together with the controller driver.

This backend covers devices exposed through XInput. A music-game instrument
that exposes only a USB HID interface will need a separate device backend and
mapping profile; Xbox-style UI prompts do not imply that every guitar model is
already supported.

Version 0.49.0 renders the gameplay fallback mapping directly over the highway:
A, B, Y, X, and RB correspond to green, red, yellow, blue, and orange frets;
D-pad up/down are strum. The runtime retains a connected controller slot across
frames, falls back when it disconnects, and detects hotplug without restarting.
