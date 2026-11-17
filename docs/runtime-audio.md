# Runtime PCM WAVE loader

Version 0.8.0 loads RIFF/WAVE metadata through the read-only archive VFS. It
walks logical chunks, requires the observed PCM format tag 1, and exposes channel
count, sample rate, byte rate, block alignment, bit depth, data range, frame
count, and duration.

PCM bytes remain in the user's ARK files and are read only in caller-requested,
record-bounded ranges. This milestone does not provide an audio device, mixer,
resampling, streaming queue, MOGG decoding, or synchronization with gameplay.
