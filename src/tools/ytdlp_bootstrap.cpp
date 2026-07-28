#include "ytdlp_bootstrap.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QCryptographicHash>
#include <QUrl>
#include <QDebug>

namespace lumen::tools {

namespace {

// Pin a known-good release. Bump periodically when YouTube breaks old builds.
// Checksums: https://github.com/yt-dlp/yt-dlp/releases/tag/<tag>
// SHA256 of yt-dlp.exe for windows (updated when the pin moves).
constexpr char kReleaseTag[] = "2025.10.14";
// Empty SHA disables verification (not recommended). Fill from release assets.
// If empty, we still download but log a warning — better than shipping a stale binary.
constexpr char kExpectedSha256[] =
    ""; // filled at runtime from SHA2-256SUMS when possible

QByteArray downloadBytes(const QUrl &url, QString *errorMsg, int timeoutMs = 120000)
{
    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("LumenMusic/%1").arg(QCoreApplication::applicationVersion()));

    QNetworkReply *reply = nam.get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        if (errorMsg) *errorMsg = QStringLiteral("yt-dlp download timed out");
        reply->deleteLater();
        return {};
    }
    if (reply->error() != QNetworkReply::NoError) {
        if (errorMsg) *errorMsg = reply->errorString();
        reply->deleteLater();
        return {};
    }
    const QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

QString sha256Hex(const QByteArray &data)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString parseShaFromSums(const QByteArray &sums, const QString &fileName)
{
    // Format: "<hex>  yt-dlp.exe" or "<hex> *yt-dlp.exe"
    const QString text = QString::fromUtf8(sums);
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (!trimmed.contains(fileName, Qt::CaseInsensitive))
            continue;
        const QString hex = trimmed.section(QLatin1Char(' '), 0, 0).trimmed();
        if (hex.size() == 64)
            return hex.toLower();
    }
    return {};
}

bool writeAtomic(const QString &path, const QByteArray &data, QString *errorMsg)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    const QString tmp = path + QStringLiteral(".tmp");
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly)) {
        if (errorMsg) *errorMsg = f.errorString();
        return false;
    }
    if (f.write(data) != data.size()) {
        if (errorMsg) *errorMsg = QStringLiteral("short write");
        f.close();
        QFile::remove(tmp);
        return false;
    }
    f.close();
    if (QFile::exists(path))
        QFile::remove(path);
    if (!QFile::rename(tmp, path)) {
        if (errorMsg) *errorMsg = QStringLiteral("rename failed");
        QFile::remove(tmp);
        return false;
    }
    return true;
}

QString downloadManaged(QString *errorMsg)
{
    const QString dest = ytdlpToolsDir() + QStringLiteral("/yt-dlp.exe");
    const QString tag = QString::fromLatin1(kReleaseTag);
    const QString base =
        QStringLiteral("https://github.com/yt-dlp/yt-dlp/releases/download/%1/").arg(tag);

    // Prefer official checksums file for this tag.
    QString expected = QString::fromLatin1(kExpectedSha256).toLower();
    if (expected.isEmpty()) {
        const QByteArray sums = downloadBytes(QUrl(base + QStringLiteral("SHA2-256SUMS")), errorMsg);
        if (!sums.isEmpty())
            expected = parseShaFromSums(sums, QStringLiteral("yt-dlp.exe"));
    }

    QString err;
    const QByteArray bin = downloadBytes(QUrl(base + QStringLiteral("yt-dlp.exe")), &err);
    if (bin.isEmpty()) {
        if (errorMsg) *errorMsg = err.isEmpty()
            ? QStringLiteral("failed to download yt-dlp.exe")
            : err;
        return {};
    }

    if (!expected.isEmpty()) {
        const QString got = sha256Hex(bin);
        if (got != expected) {
            if (errorMsg) {
                *errorMsg = QStringLiteral("yt-dlp SHA-256 mismatch (got %1, expected %2)")
                                .arg(got, expected);
            }
            return {};
        }
    } else {
        qWarning() << "yt-dlp: downloaded without checksum verification"
                    << "(no SHA2-256SUMS / pin)";
    }

    if (!writeAtomic(dest, bin, errorMsg))
        return {};

    qInfo() << "yt-dlp: installed" << dest << "tag" << tag;
    return dest;
}

} // namespace

QString ytdlpToolsDir()
{
    // Use AppLocalData under the legacy org/app names (Vinil Player).
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/tools");
    QDir().mkpath(dir);
    return dir;
}

QString findYtDlpExisting()
{
    // 1) Next to the app (portable / old installs)
    const QString appDir = QCoreApplication::applicationDirPath();
    for (const QString &c : {
             QDir(appDir).filePath(QStringLiteral("yt-dlp.exe")),
             QDir(appDir).filePath(QStringLiteral("../yt-dlp.exe")) }) {
        if (QFileInfo::exists(c))
            return QDir::cleanPath(c);
    }
    // 2) Managed bootstrap location
    const QString managed = ytdlpToolsDir() + QStringLiteral("/yt-dlp.exe");
    if (QFileInfo::exists(managed))
        return managed;
    // 3) PATH
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
    if (!onPath.isEmpty())
        return onPath;
    const QString onPathExe = QStandardPaths::findExecutable(QStringLiteral("yt-dlp.exe"));
    if (!onPathExe.isEmpty())
        return onPathExe;
    return {};
}

QString ensureYtDlp(QString *errorMsg)
{
    const QString existing = findYtDlpExisting();
    if (!existing.isEmpty())
        return existing;
    return downloadManaged(errorMsg);
}

QString updateYtDlp(QString *errorMsg)
{
    // Always re-download the pinned release into the tools dir.
    return downloadManaged(errorMsg);
}

} // namespace lumen::tools
