#ifndef LUMEN_YTDLP_BOOTSTRAP_H
#define LUMEN_YTDLP_BOOTSTRAP_H

#include <QString>
#include <QRegularExpression>
#include <functional>

// First-run / on-demand yt-dlp bootstrap (P8.4).
// No binary is committed to git; the helper is downloaded from the official
// yt-dlp GitHub release with SHA-256 verification.
namespace lumen::tools {

// Directory under AppLocalData where the managed binary lives.
QString ytdlpToolsDir();

// Path of a usable yt-dlp.exe if one already exists.
// Preference: managed tools dir, then next to the exe, then PATH.
QString findYtDlpExisting();

// Ensure a yt-dlp binary is available. Downloads + verifies when missing.
// Returns absolute path on success, empty string on failure (errorMsg filled).
// safe to call from the GUI thread (blocks with a nested event loop).
QString ensureYtDlp(QString *errorMsg = nullptr);

// Version-check + auto-update used by download flows. If the managed binary
// is older than the pin, re-downloads (rate-limited). statusCb receives a
// Portuguese UI string the caller should Lang::tr().
QString ensureFreshYtDlp(QString *errorMsg = nullptr,
                         const std::function<void(const QString &ptStatus)> &statusCb = {});

// After a download process exits ≠ 0: update the pin once this session and
// return the new path so the caller can retry. Empty = do not retry.
QString updateYtDlpAfterFailure(QString *errorMsg = nullptr);

// Optional: re-download even if present (manual "update yt-dlp").
QString updateYtDlp(QString *errorMsg = nullptr);

// Compare yt-dlp versions (AAAA.MM.DD). <0 installed older than pin, 0 equal,
// >0 installed newer. Malformed installed is treated as outdated (returns -1).
inline int compareYtDlpVersion(const QString &installed, const QString &pin)
{
    auto parse = [](const QString &s, int &y, int &m, int &d) -> bool {
        static const QRegularExpression re(
            QStringLiteral(R"((\d{4})\.(\d{1,2})\.(\d{1,2}))"));
        const auto match = re.match(s);
        if (!match.hasMatch()) return false;
        y = match.captured(1).toInt();
        m = match.captured(2).toInt();
        d = match.captured(3).toInt();
        return m >= 1 && m <= 12 && d >= 1 && d <= 31;
    };
    int iy = 0, im = 0, id = 0, py = 0, pm = 0, pd = 0;
    if (!parse(pin, py, pm, pd))
        return 0;
    if (!parse(installed, iy, im, id))
        return -1;
    if (iy != py) return iy < py ? -1 : 1;
    if (im != pm) return im < pm ? -1 : 1;
    if (id != pd) return id < pd ? -1 : 1;
    return 0;
}

} // namespace lumen::tools

#endif // LUMEN_YTDLP_BOOTSTRAP_H
