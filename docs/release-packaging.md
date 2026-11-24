# Release packaging

Version 0.70.0 introduces a reproducible CMake release preset and ZIP package.
The package contains the graphical launcher, builder, song verification and
playback tools, texture converter, project documentation, official branding,
and an empty `songs/` directory for user-supplied content.

Run the following commands from the repository root:

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release
cpack --preset release
```

The archive name includes the project version, platform, and architecture, such
as `rb4pc-0.70.0-win-x64.zip`. No game content is copied into the package.
