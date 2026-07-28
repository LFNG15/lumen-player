# Security Policy

## Supported versions

| Version | Supported |
|---------|-----------|
| 2.x     | Yes       |
| 1.x     | Security fixes only as capacity allows |

## Reporting a vulnerability

Please **do not** open a public issue for security vulnerabilities that could
harm users (e.g. path traversal, code execution via media, installer supply
chain).

Prefer one of:

1. GitHub **Security Advisories** on the repository (private report), or
2. Contact the maintainers through the Lumen Connection organization on GitHub.

Include:

- Affected version / commit
- Reproduction steps
- Impact assessment
- Any suggested fix (optional)

We aim to acknowledge reports within **7 days** and to provide a timeline once
the issue is confirmed.

## Scope notes

- The app is **offline-first** and stores data under
  `%LOCALAPPDATA%\VinilPlayer\Vinil Player\` (legacy names — intentional).
- `yt-dlp` is downloaded on demand from the official GitHub releases with
  checksum verification when possible. Treat third-party binaries as untrusted
  until verified.
- Installers are **not code-signed** yet; verify SHA-256 sums published with
  each GitHub Release.
