# Architecture — Lumen Music v2

High-level map for contributors. For historical constraints and *why* decisions
were made, see `ContextProject.md` when available.

## Product

Local-only music player and library manager for **Windows x64**. Offline, no
accounts, no telemetry. License: **AGPL-3.0**.

Storage identifiers remain legacy on purpose:

| Setting | Value | Effect |
|---------|-------|--------|
| `organizationName` | `VinilPlayer` | `%LOCALAPPDATA%\VinilPlayer\…` |
| `applicationName` | `Vinil Player` | DB + QSettings path |

## Stack

| Layer | Choice |
|-------|--------|
| Language | C++20 |
| UI | Qt 6 Widgets (no QML / `.ui`) |
| Audio | Qt Multimedia (FFmpeg backend for `.opus`) |
| DB | SQLite via Qt Sql (WAL) |
| Build | CMake ≥ 3.21 + Ninja + MSVC |
| Windows media | C++/WinRT SMTC (`src/platform/`) |

## Source layout

```
src/
  main.cpp                 App entry, CLI flags, cold-start probe
  mainwindow.*             Shell: sidebar, stack, dialogs, platform glue
  database/                SQLite access + Migrator (user_version)
  design/                  Tokens, palettes, ThemeManager, QSS, i18n, paint
  models/                  TrackListModel, delegate, filter proxy, context menu
  pages/                   Feature screens
  player/                  TrackModel, PlaybackEngine, PlayerBar (view)
  platform/                SMTC / media keys / tray (no winrt in headers)
  tools/                   yt-dlp bootstrap
  widgets/                 Small shared widgets + shims (theme.h, lang.h)
```

## Runtime data flow

```
TrackModel  ←→  Database  ←→  vinil.db (migrations on open)
     ↑
TrackListModel / pages
     ↑
PlaybackEngine ←→ QMediaPlayer
     ↑
PlayerBar (UI)    NowPlaying (SMTC)    TrayIcon
```

## Schema (user_version = 2)

- `playlists` — name, cover, **`dir_name`** (disk folder, stable on rename), sort_mode  
- `tracks` — file_path, **owner_playlist_id** (advisory), liked/liked_at, no position column  
- `playlist_tracks` — N:N membership + **position** (gap 1024)  
- `playback_state` — track, pos, volume, mute, shuffle, repeat, queue id lists  

Migrations: `src/database/migrator.*`. Failures refuse to open the app; backups
as `vinil.db.v{N}.bak` after WAL checkpoint.

## Design system

All visual tokens live under `src/design/`. Runtime styling goes through
`StyleSheet::apply` / `applyApp` only. `src/widgets/theme.h` and `lang.h` are
thin shims.

Gate: no `setStyleSheet` outside `src/design/`.

## Out of scope for 2.0

TagLib, Album/Artist pages, automatic discovery playlists, QML rewrite, moving
user audio files on disk. See roadmap in `README.md` / `CHANGELOG.md`.
