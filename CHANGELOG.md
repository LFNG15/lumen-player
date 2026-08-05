# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned for 2.1

- TagLib / embedded cover art
- Album and Artist pages
- Local discovery shelves (opt-in)

## [2.0.0] - 2026-07-28

### Added

- Design system with live theme switching (6 palettes × dark/light/high-contrast × comfortable/compact)
- Live language switching (PT/EN) without app restart
- N:N playlist membership (`playlist_tracks`) and advisory owner playlist
- Virtualized track lists (`QListView` + delegate) with multi-select, context menu, and keyboard shortcuts
- `PlaybackEngine` extracted from the player bar (shuffle bag, repeat off/all/one)
- Windows SMTC integration, media-key fallback, and system tray
- Versioned database migrations (`PRAGMA user_version`) with WAL-safe backups
- First-run / on-demand **yt-dlp** download with checksum verification (binary no longer in git)
- CI (Windows MSVC), unit tests (QTest), packaging improvements

### Fixed

- Playlist custom order destroyed on every boot (Issue #2)
- Drag-and-drop source row deletion in reorder lists
- Volume restored from QSettings while also written to DB (DB is now source of truth)

### Changed

- Toolchain: **MinGW → MSVC**, C++20
- Inno Setup script: pinned AppId, English + PT-BR, setup icon
- CMake deploy: config-aware `windeployqt` with plugin gates

### Removed

- Dead `LumenMusic.pro` (qmake)
- Bundled `src/thirdparty/yt-dlp.exe` from version control

## [1.1.1] - prior

See git history prior to the v2.0.0 branch work.
