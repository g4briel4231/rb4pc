# Runtime window and software framebuffer

Version 0.15.0 adds a platform-neutral BGRA framebuffer with checked pixel
writes and a Win32 presentation backend using `StretchDIBits`. The explicit
`rb4pc-window` diagnostic renders a generated gradient and processes the native
message loop until the user closes it.

No window is opened by tests. This milestone does not parse game textures,
render 3D scenes, provide fonts or UI widgets, or implement gameplay visuals.
