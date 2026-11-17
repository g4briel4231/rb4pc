# Integrated pre-alpha shell

Version 0.20.0 is the first explicit visual integration target. It boots a
validated HDR/ARK source, creates the runtime session and shell model, renders a
generated status/menu panel into the software framebuffer, and presents it in
the Win32 window.

The executable is intentionally named `rb4pc-prealpha`. It has no text renderer,
song chart loader, multitrack audio, instrument mapping, gameplay, or original
game assets. Automated tests render only to memory and never open the window.
