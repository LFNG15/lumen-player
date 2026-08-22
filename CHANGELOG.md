# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned for 2.1

- TagLib / embedded cover art
- Album and Artist pages
- Local discovery shelves (opt-in)

## [2.0.1] - 2026-08-22

Patch release after v2.0.0: session restore, YouTube downloads, live language, contrast, and a few quality-of-life extras.

### Highlights

- **Your session actually comes back.** Queue, current track, playback position, and volume now survive closing and reopening the app — including existing v2.0.0 libraries. Volume changes from the mouse wheel or keyboard are saved. State is also written every 30 seconds and on Windows logoff, so a crash or shutdown does not wipe the session.
- **Play after restore works immediately.** After a restart you can press Play and hear audio from the saved position. You no longer have to drag the seek bar to “kick” playback. If the file is missing, the app shows a toast instead of staying silent.
- **YouTube downloads stay current.** yt-dlp is pinned to **2026.08.19**, the copy Lumen Music manages is preferred over a stale exe next to the app, outdated binaries auto-update, and a failed download retries once after updating.

### New

- **Like from the player bar** (#19) — a heart between Next and Repeat. It stays in sync with Liked Songs; it hides with Shuffle/Repeat on a narrow window.
- **Time-aware Home greeting** (#22) — five bands (late night, morning, afternoon, evening, late evening) with several casual lines each, in PT and EN. The greeting scales and wraps when you resize the window.
- **Back arrows** (#24) — Playlists and Full Library now have the same Back control as playlist detail, using real navigation history. The Add page uses the same icon.

### Fixed

- **Edit / create / rename dialogs** (#23) — height follows the layout so fields and buttons are not clipped at 125% Windows scaling.
- **High contrast** (#25) — turning it on is obvious: dark users get a near-black HC look, light users get a near-white one. Pure backgrounds, stronger borders, visible hover and focus.
- **Light mode** (#18) — clearer card borders, card hover, and AA text contrast.
- **Language switch** (#26) — switching PT↔EN updates the sidebar, search, Home, Library, player bar, Add Music, and playlist detail without restarting.
- **App icon** — the Lumen Music mark uses a transparent `.ico` on the taskbar and desktop shortcut (no black square).

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
