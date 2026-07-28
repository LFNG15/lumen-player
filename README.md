<div align="center">

# 🔶 Lumen Music

**A lightweight, elegant desktop music player built in C++ with Qt.**

Local library · playlists · queue · listening history · live themes · Windows SMTC

[![CI](https://github.com/Lumen-Connection/lumen-music/actions/workflows/ci.yml/badge.svg)](https://github.com/Lumen-Connection/lumen-music/actions/workflows/ci.yml)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)
![Qt](https://img.shields.io/badge/Qt-6.x%20MSVC-41CD52?logo=qt&logoColor=white)
![Platform](https://img.shields.io/badge/Windows-x64-0078D6?logo=windows&logoColor=white)
![License](https://img.shields.io/badge/license-AGPL--3.0-blue)

</div>

---

## About

**Lumen Music** is a local audio player focused on a clean, fluid experience.
Import files (or fetch audio via yt-dlp), organize playlists, like tracks, and
control everything through a themed interface. No cloud, no accounts —
everything stays on your machine.

Part of the [Lumen Connection](https://lumenconnection.com.br) family.

## Features (v2.0)

### Library and organization
- Import local audio; download from YouTube (yt-dlp on first use)
- Playlists with cover gradient or image; reorder via drag-and-drop (order persists)
- N:N membership — a track can live in multiple playlists without duplicating files
- Liked songs collection; edit title/artist; multi-select + context menu
- Import Spotify / YouTube playlists (heuristic match)

### Playback
- Full player: play/pause, prev/next, shuffle bag, repeat off/all/one, volume/mute
- Spotify-style queue; context-aware next/prev per playlist / liked / library
- Restores track, position, queue, and shuffle after restart
- Windows **SMTC** (media keys, Bluetooth headphones, system flyout)

### Look and feel
- **6 palettes** × dark / light / high-contrast × comfortable / compact
- Live theme and language switch (PT/EN) without restart
- Optional reduce-motion for animations

## Tech stack

| | |
|---|---|
| Language | C++20 |
| UI | Qt 6 Widgets (Multimedia, SQL, Network) |
| Build | **CMake + Ninja + MSVC 2022** |
| Storage | SQLite (WAL, versioned migrations) |

> MinGW is **not** supported for v2.x (SMTC needs MSVC + C++/WinRT).

## Building

Follow **[CONTRIBUTING.md](CONTRIBUTING.md)** for the supported setup. Short version:

### Prerequisites
- MSVC 2022 (Build Tools or VS) — x64
- CMake ≥ 3.21, Ninja
- Qt 6.10+ **`msvc2022_64`** with **Multimedia** (FFmpeg), **Sql**, **Network**
- Windows SDK with C++/WinRT (for SMTC; or build with `-DLUMEN_WITH_SMTC=OFF`)

### Configure and build

```powershell
# From "x64 Native Tools Command Prompt for VS 2022" (or after vcvars64.bat)
$env:QTDIR = "C:/Qt/6.11.0/msvc2022_64"   # adjust to your install

cmake --preset msvc-ninja
cmake --build --preset msvc-ninja
ctest --test-dir build/msvc-ninja --output-on-failure
.\build\msvc-ninja\LumenMusic.exe
```

Optional local preset: copy `CMakeUserPresets.json.example` → `CMakeUserPresets.json`
and set `QTDIR`.

### Useful CLI flags

| Flag | Purpose |
|------|---------|
| `--i18n-canary` | Exit 0 if English i18n works (guards MSVC `/utf-8`) |
| `--selftest path\to\file.opus` | Exit 0 if media loads (deploy smoke) |
| `--seed-fake-library N` | Insert N synthetic tracks for perf testing |

## Packaging

```powershell
# Self-contained folder (windeployqt + plugin gates)
cmake --install build/msvc-ninja --prefix dist\LumenMusic

# ZIP (CPack)
cpack --config build\msvc-ninja\CPackConfig.cmake -G ZIP -B dist

# Installer (Inno Setup) — expects dist\LumenMusic\
iscc installer\lumen-music.iss
```

`yt-dlp` is **not** bundled. On first YouTube import the app downloads a pinned
release into `%LOCALAPPDATA%\VinilPlayer\Vinil Player\tools\` with checksum
verification when available.

Tagged releases (`v*`) publish ZIP + setup + SHA-256 via GitHub Actions
(see `.github/workflows/release.yml`).

## Documentation

| Doc | Content |
|-----|---------|
| [CONTRIBUTING.md](CONTRIBUTING.md) | Build, tests, commit/branch conventions |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Source map and data flow |
| [CHANGELOG.md](CHANGELOG.md) | Keep a Changelog |
| [SECURITY.md](SECURITY.md) | Vulnerability reporting |
| [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) | Community standards |

## Roadmap (v2.1+)

- TagLib / embedded cover art
- Album and Artist pages
- Optional local discovery shelves

## License

Built by [Lumen Connection](https://lumenconnection.com.br), distributed under
the [AGPL-3.0](LICENSE) license.

---

<div align="center">
Made with C++ and Qt • <strong>🄯 Lumen Connection</strong>
</div>
