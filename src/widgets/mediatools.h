#ifndef MEDIATOOLS_H
#define MEDIATOOLS_H

#include <QString>
#include <QStringList>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

// Shared helpers for everything that downloads audio through the bundled
// yt-dlp (the Add Music page and the streaming-playlist importer).
namespace MediaTools {

// Audio extensions the app accepts. Opus is the preferred format, but the Qt
// ffmpeg multimedia backend (bundled) also plays these — so YouTube downloads
// can land as native .webm/Opus or .m4a/AAC when ffmpeg isn't installed to
// remux them into a clean .opus.
inline const QStringList &audioExts() {
    static const QStringList exts =
        {"opus", "webm", "m4a", "mp3", "ogg", "oga", "flac", "wav", "aac"};
    return exts;
}

inline QString findFfmpeg() {
    QString found = QStandardPaths::findExecutable("ffmpeg");
    if (!found.isEmpty()) return found;

    QString localAppData = qgetenv("LOCALAPPDATA");
    QString userProfile  = qgetenv("USERPROFILE");

    QStringList candidates;

    // WinGet (Gyan.FFmpeg)
    QDir winget(localAppData + "/Microsoft/WinGet/Packages");
    for (const auto &pkg : winget.entryList({"Gyan.FFmpeg*"}, QDir::Dirs)) {
        QDir pkgDir(winget.filePath(pkg));
        for (const auto &sub : pkgDir.entryList({"ffmpeg*"}, QDir::Dirs))
            candidates << pkgDir.filePath(sub) + "/bin/ffmpeg.exe";
    }

    // Scoop
    candidates << userProfile + "/scoop/apps/ffmpeg/current/bin/ffmpeg.exe";
    // Chocolatey
    candidates << "C:/ProgramData/chocolatey/bin/ffmpeg.exe";
    // Manual
    candidates << "C:/ffmpeg/bin/ffmpeg.exe";

    for (const auto &c : candidates)
        if (QFile::exists(c)) return c;
    return {};
}

// Locate an existing yt-dlp only (no download). Prefer ensureYtDlp() from
// tools/ytdlp_bootstrap.h when the binary may need to be fetched (P8.4).
inline QString findYtDlp() {
    const QString appDir = QCoreApplication::applicationDirPath();
    for (const QString &candidate : {
             QDir(appDir).filePath("yt-dlp.exe"),
             QDir(appDir).filePath("../yt-dlp.exe") }) {
        if (QFileInfo::exists(candidate)) return QDir::cleanPath(candidate);
    }
    // Managed first-run location
    const QString managed =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/tools/yt-dlp.exe");
    if (QFileInfo::exists(managed)) return managed;
    // PATH fallback
    QString onPath = QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
    if (!onPath.isEmpty()) return onPath;
    onPath = QStandardPaths::findExecutable(QStringLiteral("yt-dlp.exe"));
    if (!onPath.isEmpty()) return onPath;
    return {};
}

inline QString downloadDir() {
    QSettings settings;
    QString fallback =
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + "/Lumen Music";
    QString dir = settings.value("downloadDir").toString();
    if (dir.isEmpty()) dir = fallback;
    QDir().mkpath(dir);
    return dir;
}

// Strips characters Windows forbids in folder/file names.
inline QString sanitizeFileName(QString name) {
    static const QRegularExpression bad(R"([\\/:*?"<>|])");
    name.replace(bad, "_");
    return name.trimmed();
}

// Per-playlist subfolder under the downloads root. Every playlist gets its
// own folder so the audio files are easy to locate in the file explorer.
// Created on demand; falls back to the root for unnamed playlists.
inline QString playlistDir(const QString &playlistName) {
    QString dir = downloadDir();
    const QString sub = sanitizeFileName(playlistName);
    if (!sub.isEmpty()) dir += "/" + sub;
    QDir().mkpath(dir);
    return dir;
}

// yt-dlp arguments to fetch one audio source (URL or ytsearch query). With
// ffmpeg available the stream is remuxed into a clean .opus; without it the
// native audio stream is kept as-is (still playable by the Qt backend).
inline QStringList downloadArgs(const QString &source, const QString &outTemplate) {
    const QString ffmpeg = findFfmpeg();
    QStringList args;
    if (!ffmpeg.isEmpty()) {
        args = {
            "-x",
            "--audio-format", "opus",
            "--audio-quality", "0",
            "--ffmpeg-location", ffmpeg,
            "--no-playlist",
            "-o", outTemplate,
            source
        };
    } else {
        args = {
            "-f", "bestaudio[acodec=opus]/bestaudio",
            "--no-playlist",
            "-o", outTemplate,
            source
        };
    }
    return args;
}

}  // namespace MediaTools

#endif // MEDIATOOLS_H
