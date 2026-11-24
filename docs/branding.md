# RB4PC branding

The two official logo variants and both repository-icon iterations are stored
in `assets/branding`. The project currently uses `rb4pc-logo2.png` at the top of
the repository README and `rb4pc-icon2.png` as the selected application and
repository icon.

`assets/windows/rb4pc.ico` is a 256x256 Windows icon generated from the selected
PNG. `assets/windows/rb4pc.rc` embeds it as `IDI_RB4PC_ICON`. CMake attaches this
resource to every shipped Windows executable so Explorer, shortcuts, and native
windows use one consistent project identity.
