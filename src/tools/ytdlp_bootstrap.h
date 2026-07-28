#ifndef LUMEN_YTDLP_BOOTSTRAP_H
#define LUMEN_YTDLP_BOOTSTRAP_H

#include <QString>

// First-run / on-demand yt-dlp bootstrap (P8.4).
// No binary is committed to git; the helper is downloaded from the official
// yt-dlp GitHub release with SHA-256 verification.
namespace lumen::tools {

// Directory under AppLocalData where the managed binary lives.
QString ytdlpToolsDir();

// Path of a usable yt-dlp.exe if one already exists (app dir, PATH, tools dir).
// Does not download.
QString findYtDlpExisting();

// Ensure a yt-dlp binary is available. Downloads + verifies when missing.
// Returns absolute path on success, empty string on failure (errorMsg filled).
// safe to call from the GUI thread (blocks with a nested event loop).
QString ensureYtDlp(QString *errorMsg = nullptr);

// Optional: re-download even if present (manual "update yt-dlp").
QString updateYtDlp(QString *errorMsg = nullptr);

} // namespace lumen::tools

#endif // LUMEN_YTDLP_BOOTSTRAP_H
