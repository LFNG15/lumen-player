# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned

- TagLib / embedded cover art
- Album and Artist pages
- Local discovery shelves (opt-in)
- QR code in the sync dialog, so pairing does not need the PIN typed by hand

## [2.1.0] - 2026-08-25

Minor release: Lumen Music can now serve your library to the phone over your own Wi-Fi.

### Highlights

- **Sync with [Lumen Music Mobile](https://github.com/Lumen-Connection/lumen-music-mobile).** Turn syncing on from the sidebar, type the PIN on your phone, and your library — playlists, covers, liked songs, play counts — appears there. Audio files travel too, for the playlists you pick on the phone.
- **Your PC stays the source of truth.** Files and the catalogue only ever go one way: to the phone. Deleting a track here removes it there, file and all. Only light state travels back — likes, play counts, and playlists you created on the phone.
- **Off by default.** A music player has no business listening on the network until you ask it to. The server runs only while you want it to, and paired devices can be revoked at any time from the same dialog.

### New

- **Sync dialog** — on/off switch, the address to type when automatic discovery is blocked, the pairing PIN, and the list of paired devices with a revoke button.
- **Pick up where you left off.** The phone can resume the track you were playing here, at the same position.

### Technical

- New `src/sync` module: a minimal HTTP/1.1 server on `QTcpServer` (the Qt kit ships without the optional `QHttpServer` add-on), UDP probe/response discovery, PIN pairing, library snapshot, and an additive merge service.
- Schema migration to `user_version = 3`: adds `sync_meta` and `sync_devices` plus `playlists.origin_device`. Additive only — no existing table is touched, and the migrator still backs up before running.
- The server runs on its own thread with its own SQL connection, so serialising the library or streaming a whole track never blocks the interface. A push from the phone reloads the open views: the desktop is no longer the only writer of the database.
- Only the SHA-256 of a device token is stored, so a leaked `vinil.db` cannot be turned into a working credential. The pairing PIN is burned on success and after five wrong attempts.
- Six new QTest targets covering pairing, HTTP range requests, the snapshot, the merge rules, and an end-to-end run of the whole server.

### Note

The first time you turn syncing on, Windows Firewall will ask for permission — allow it for **private networks**. There is no TLS on the local network in this version: the connection is authenticated by token instead.

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
