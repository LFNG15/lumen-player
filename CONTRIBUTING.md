# Contributing to Lumen Music

Thanks for helping. This document is the **supported** way to build and contribute on Windows x64.

## Prerequisites

| Tool | Notes |
|------|--------|
| **MSVC 2022** Build Tools or VS 2022 | C++ workload, x64 |
| **CMake** ≥ 3.21 | On `PATH` |
| **Ninja** | VS ships one under `Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja` |
| **Qt 6.11+ `msvc2022_64`** | Modules: **Qt Base**, **Qt Multimedia** (FFmpeg backend), **Qt Sql** |
| **Windows SDK** (with C++/WinRT) | For SMTC; optional if you set `LUMEN_WITH_SMTC=OFF` |

Do **not** use MinGW for v2.x — the project targets MSVC for SMTC.

### Qt install (example with aqtinstall)

```powershell
pip install aqtinstall
aqt install-qt windows desktop 6.11.0 win64_msvc2022_64 -O C:\Qt -m qtmultimedia
```

If 6.11.x is unavailable on mirrors, 6.10.x `msvc2022_64` also works for local builds.

## Local presets

Copy the example and set your Qt path:

```powershell
copy CMakeUserPresets.json.example CMakeUserPresets.json
# edit QTDIR → e.g. C:/Qt/6.11.0/msvc2022_64
```

`CMakeUserPresets.json` is gitignored.

## Build

From an **x64 Native Tools** / `vcvars64.bat` environment:

```powershell
$env:QTDIR = "C:/Qt/6.11.0/msvc2022_64"   # or rely on CMakeUserPresets "local"
cmake --preset msvc-ninja
cmake --build --preset msvc-ninja
```

Debug:

```powershell
cmake --preset msvc-ninja-debug
cmake --build --preset msvc-ninja-debug
```

Run:

```powershell
.\build\msvc-ninja\LumenMusic.exe
# or after install:
cmake --install build/msvc-ninja --prefix "$PWD/dist/LumenMusic"
.\dist\LumenMusic\LumenMusic.exe
```

Useful flags:

| Flag | Purpose |
|------|---------|
| `--i18n-canary` | Exit 0 if EN translations work (guards `/utf-8`) |
| `--selftest path\to\file.opus` | Exit 0 if media loads |
| `--seed-fake-library N` | Insert N synthetic tracks for perf testing |

## Tests

```powershell
cmake --build --preset msvc-ninja
ctest --test-dir build/msvc-ninja --output-on-failure
```

Tests cover accent normalization, position-gap math, delegate hit-testing, and schema migrations.

## Coding conventions

### Commits — [Conventional Commits](https://www.conventionalcommits.org/)

```
feat: add shuffle bag for playlist order
fix: preserve playlist order across restarts
build: migrate toolchain to MSVC
docs: document CMake presets
chore: remove dead qmake project
```

Use the imperative mood; English messages.

### Branches — ASCII only

```
feat/short-description
fix/issue-number-short-description
chore/tooling
```

No accents, cedillas, or spaces in branch names.

### Design / style

- After P1: **no** `setStyleSheet` outside `src/design/`.
- Do not rename `applicationName` / `organizationName` (`Vinil Player` / `VinilPlayer`) — they own the DB and QSettings paths.
- Do not introduce TagLib, album/artist pages, or genre columns in v2.0.x (deferred to v2.1).
- Never move or copy user audio files (owner playlist is advisory only).

### Pull requests

- Fill the PR template checklist.
- Keep PRs focused; prefer a stack of small conventional commits.
- CI must be green (Windows MSVC build + tests + i18n canary).

## License

By contributing you agree that your contributions are licensed under the project’s **AGPL-3.0** license (see `LICENSE`).
