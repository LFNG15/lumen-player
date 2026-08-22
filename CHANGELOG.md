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

### Fixed

- **#20 / #21** — fila, última faixa, posição e volume agora sobrevivem ao restart. Causa principal: `QString` nulo no `join` de listas vazias fazia o `UPDATE` de `playback_state` falhar inteiro (NOT NULL). Volume da roda do mouse/teclado passa a chegar no engine; contexto efetivo é persistido; salvamento a cada 30s e no logoff do Windows.
- **Play sem seek** — após restaurar a sessão, o play não ficava mudo até o usuário avançar a barra. A mídia só é carregada no primeiro play (lazy-load); mídia inválida mostra toast.
- **#23** — diálogo de edição (e criar/renomear playlist) deixa a altura com o layout; campos não cortam em scaling 125%.
- **#25** — Alto Contraste é visivelmente distinto: HC-escuro (fundos pretos) a partir do dark, HC-claro (fundos brancos) a partir do light.
- **#18** — modo light: bordas mais visíveis, hover de card via `cardHover`, contraste de texto AA.
- **#26** — troca PT↔EN ao vivo retraduz sidebar, busca, Home, Biblioteca, PlayerBar, Adicionar e detalhe de playlist (bindings existentes, com guarda contra widget destruído).
- **Download YouTube** — pin do yt-dlp atualizado para `2026.08.19`; o binário gerenciado tem prioridade; auto-update por versão e retry único após falha.

### Added

- **#19** — botão Favoritar (coração) na PlayerBar, entre next e repeat; some junto de shuffle/repeat em janela estreita.
- **#22** — saudação da Home com 5 faixas (madrugada / manhã / tarde / noite / noite alta) e 3 variações.
- **#24** — seta Voltar em Playlists e Biblioteca Completa (histórico real); Adicionar usa o mesmo ícone.

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
