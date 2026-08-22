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
#include <QProcess>
#include <QSettings>
#include <QDateTime>
#include <functional>

namespace lumen::tools {

namespace {

// Pin a known-good release. Bump periodically when YouTube breaks old builds.
// Checksums: https://github.com/yt-dlp/yt-dlp/releases/tag/<tag>
// SHA256 of yt-dlp.exe for windows (updated when the pin moves).
constexpr char kReleaseTag[] = "2026.08.19";
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

constexpr qint64 kMinUpdateIntervalMs = 6LL * 60 * 60 * 1000;
const char kLastUpdateKey[] = "ytdlpLastUpdateCheck";
bool g_updatedThisSession = false;

void markUpdateAttempt()
{
    g_updatedThisSession = true;
    QSettings s;
    s.setValue(QLatin1String(kLastUpdateKey), QDateTime::currentMSecsSinceEpoch());
}

bool shouldAttemptProactiveUpdate()
{
    if (g_updatedThisSession)
        return false;
    QSettings s;
    const qint64 last = s.value(QLatin1String(kLastUpdateKey)).toLongLong();
    if (last <= 0)
        return true;
    return (QDateTime::currentMSecsSinceEpoch() - last) >= kMinUpdateIntervalMs;
}

QString queryYtDlpVersion(const QString &binary)
{
    QProcess p;
    p.setProgram(binary);
    p.setArguments({QStringLiteral("--version")});
    p.start();
    if (!p.waitForFinished(8000)) {
        p.kill();
        p.waitForFinished(1000);
        return {};
    }
    const QString out = QString::fromUtf8(p.readAllStandardOutput() + p.readAllStandardError()).trimmed();
    return out.section(QLatin1Char('\n'), 0, 0).trimmed();
}

void emitStatus(const std::function<void(const QString &)> &statusCb, const QString &pt)
{
    if (!statusCb)
        return;
    statusCb(pt);
    QCoreApplication::processEvents();
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
    // 1) Managed bootstrap location — the app controls this copy.
    const QString managed = ytdlpToolsDir() + QStringLiteral("/yt-dlp.exe");
    if (QFileInfo::exists(managed))
        return managed;
    // 2) Next to the app (portable / old installs) — never preferred over managed.
    const QString appDir = QCoreApplication::applicationDirPath();
    for (const QString &c : {
             QDir(appDir).filePath(QStringLiteral("yt-dlp.exe")),
             QDir(appDir).filePath(QStringLiteral("../yt-dlp.exe")) }) {
        if (QFileInfo::exists(c))
            return QDir::cleanPath(c);
    }
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
    const QString downloaded = downloadManaged(errorMsg);
    if (!downloaded.isEmpty())
        markUpdateAttempt();
    return downloaded;
}

QString updateYtDlp(QString *errorMsg)
{
    // Always re-download the pinned release into the tools dir.
    const QString path = downloadManaged(errorMsg);
    if (!path.isEmpty())
        markUpdateAttempt();
    return path;
}

QString ensureFreshYtDlp(QString *errorMsg,
                         const std::function<void(const QString &ptStatus)> &statusCb)
{
    const QString pin = QString::fromLatin1(kReleaseTag);
    const QString existing = findYtDlpExisting();
    if (existing.isEmpty()) {
        emitStatus(statusCb, QStringLiteral("Atualizando ferramenta de download…"));
        return ensureYtDlp(errorMsg);
    }

    const QString ver = queryYtDlpVersion(existing);
    if (compareYtDlpVersion(ver, pin) >= 0)
        return existing;

    if (!shouldAttemptProactiveUpdate())
        return existing;

    emitStatus(statusCb, QStringLiteral("Atualizando ferramenta de download…"));
    QString err;
    const QString updated = updateYtDlp(&err);
    if (updated.isEmpty()) {
        if (errorMsg) *errorMsg = err;
        return existing; // keep the old binary rather than fail the download
    }
    return updated;
}

QString updateYtDlpAfterFailure(QString *errorMsg)
{
    if (g_updatedThisSession)
        return {};
    return updateYtDlp(errorMsg);
}

} // namespace lumen::tools
