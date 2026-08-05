# Third-party binaries

`yt-dlp.exe` is **not** stored in this repository.

On first YouTube-related import, Lumen Music downloads a pinned yt-dlp release
into the app local data folder (`…/tools/yt-dlp.exe`) with SHA-256 verification
when the official checksum file is available.

See `src/tools/ytdlp_bootstrap.cpp`.
