# Runtime archive file system

Version 0.7.0 introduces the first runtime-facing component. The read-only VFS
owns a validated HDR/ARK report and exposes exact-path existence checks, record
sizes, and bounded range reads. Lookup follows the serialized MurmurHash3 index;
reads are translated to one ARK volume and local offset.

Each request is checked against the HDR record boundary and a single call is
limited to 64 MiB. The VFS does not expose host paths to internal callers, does
not extract files, and never modifies source archives. This is infrastructure
for later loaders, not yet a game runtime or playable build.
