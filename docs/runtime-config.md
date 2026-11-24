# Runtime configuration

Version 0.90.0 reads `rb4pc.ini` from the launcher's working directory. If the
file is missing, the launcher writes validated defaults. Release packages ship
the same default file at their root.

```ini
master_volume=1.0
latency_ms=0
```

`master_volume` accepts values from `0.0` through `1.0`. `latency_ms` accepts
values from `-250` through `250` and offsets the chart clock relative to audio.
Unknown, duplicate, missing, malformed, or out-of-range settings stop startup
with an explicit diagnostic instead of being silently ignored.
