#include "sync_server.h"

#include "http_server.h"
#include "library_snapshot.h"
#include "merge_service.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkInterface>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSysInfo>
#include <QThread>

namespace lumen::sync {

namespace {

constexpr const char *kConnectionName = "lumen_sync";
constexpr const char *kTokenHeader = "x-lumen-token";

// Best-effort local IPv4 to show in the dialog, so the user can type it by hand
// when broadcast discovery is blocked.
QString primaryLocalAddress()
{
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!iface.flags().testFlag(QNetworkInterface::IsUp)
            || !iface.flags().testFlag(QNetworkInterface::IsRunning)
            || iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const auto entries = iface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            const QHostAddress address = entry.ip();
            if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback())
                return address.toString();
        }
    }
    return QStringLiteral("127.0.0.1");
}

} // namespace

class SyncServer::Impl {
public:
    HttpServer         *http = nullptr;
    DiscoveryResponder *discovery = nullptr;
    PinGuard            pinGuard;
    QString             serverId;
    QString             dbPath;
    quint16             port = kDefaultHttpPort;

    QSqlDatabase database() { return QSqlDatabase::database(QLatin1String(kConnectionName)); }
};

SyncServer::SyncServer(QObject *parent)
    : QObject(parent)
    , d(new Impl)
{
}

SyncServer::~SyncServer()
{
    stopServer();
    delete d;
}

void SyncServer::startServer(const QString &dbPath, quint16 port)
{
    if (m_running)
        return;

    d->dbPath = dbPath;
    d->port = port;

    // Own connection: a QSqlDatabase handle belongs to the thread that opened it.
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                QLatin1String(kConnectionName));
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        emit startFailed(db.lastError().text());
        return;
    }
    {
        QSqlQuery pragma(db);
        // WAL is already on from the main connection; the timeout is what keeps
        // the short merge writes from failing while the UI holds the writer.
        pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
        pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    }

    d->serverId = serverId(db);

    d->http = new HttpServer(this);
    d->http->setHandler([this](const HttpRequest &request) { return handleRequest(request); });
    if (!d->http->start(port)) {
        emit startFailed(QStringLiteral("port %1 unavailable").arg(port));
        delete d->http;
        d->http = nullptr;
        db.close();
        QSqlDatabase::removeDatabase(QLatin1String(kConnectionName));
        return;
    }

    d->discovery = new DiscoveryResponder(this);
    d->discovery->start(d->serverId, QSysInfo::machineHostName(), port);

    // Report the port actually bound: callers may pass 0 to mean "any free
    // port", and the tests rely on learning which one was chosen.
    d->port = d->http->serverPort();

    m_running = true;
    emit started(d->serverId, primaryLocalAddress(), d->port);
    requestDeviceList();
}

void SyncServer::stopServer()
{
    if (!m_running)
        return;

    d->pinGuard.clear();

    if (d->discovery) {
        d->discovery->stop();
        d->discovery->deleteLater();
        d->discovery = nullptr;
    }
    if (d->http) {
        d->http->stop();
        d->http->deleteLater();
        d->http = nullptr;
    }

    {
        QSqlDatabase db = d->database();
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(QLatin1String(kConnectionName));

    m_running = false;
    emit stopped();
}

void SyncServer::armPairing()
{
    const QString pin = generatePin();
    d->pinGuard.reset(pin);
    emit pinChanged(pin);
}

void SyncServer::disarmPairing()
{
    d->pinGuard.clear();
    emit pinChanged(QString());
}

void SyncServer::revokeDevice(const QString &deviceId)
{
    QSqlDatabase db = d->database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM sync_devices WHERE device_id = ?"));
    q.addBindValue(deviceId);
    q.exec();
    requestDeviceList();
}

void SyncServer::requestDeviceList()
{
    QVector<PairedDevice> devices;
    QSqlDatabase db = d->database();
    QSqlQuery q(db);
    if (q.exec(QStringLiteral(
            "SELECT device_id, name, created_at, last_sync_at FROM sync_devices "
            "ORDER BY created_at"))) {
        while (q.next()) {
            PairedDevice device;
            device.deviceId = q.value(0).toString();
            device.name = q.value(1).toString();
            device.createdAt = q.value(2).toLongLong();
            device.lastSyncAt = q.value(3).toLongLong();
            devices.append(device);
        }
    }
    emit deviceListChanged(devices);
}

// --- Routing -----------------------------------------------------------------

QString SyncServer::authenticate(const HttpRequest &request)
{
    const QString token = request.header(QLatin1String(kTokenHeader));
    if (token.isEmpty())
        return {};

    const QString hash = hashToken(token);
    QSqlDatabase db = d->database();
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT device_id, token_hash FROM sync_devices")))
        return {};

    // Walks every row on purpose: comparing hashes with secureEquals keeps the
    // check constant-time per row instead of letting SQL short-circuit.
    QString match;
    while (q.next()) {
        if (secureEquals(q.value(1).toString(), hash))
            match = q.value(0).toString();
    }
    return match;
}

HttpResponse SyncServer::handleRequest(const HttpRequest &request)
{
    const QString path = request.path;

    if (path == QLatin1String("/v1/ping"))
        return handlePing();

    if (path == QLatin1String("/v1/pair")) {
        if (request.method != QLatin1String("POST"))
            return HttpResponse::error(400, QStringLiteral("bad_method"));
        return handlePair(request);
    }

    // Everything below needs a paired device.
    const QString deviceId = authenticate(request);
    if (deviceId.isEmpty())
        return HttpResponse::error(401, QStringLiteral("unauthorized"));

    if (path == QLatin1String("/v1/library"))
        return handleLibrary();

    if (path == QLatin1String("/v1/push")) {
        if (request.method != QLatin1String("POST"))
            return HttpResponse::error(400, QStringLiteral("bad_method"));
        return handlePush(request, deviceId);
    }

    // /v1/tracks/{id}/file
    if (path.startsWith(QLatin1String("/v1/tracks/")) && path.endsWith(QLatin1String("/file"))) {
        const QString idText = path.mid(11, path.length() - 11 - 5);
        bool ok = false;
        const qlonglong trackId = idText.toLongLong(&ok);
        if (!ok)
            return HttpResponse::error(400, QStringLiteral("bad_id"));
        return handleTrackFile(trackId, request);
    }

    // /v1/playlists/{id}/cover
    if (path.startsWith(QLatin1String("/v1/playlists/")) && path.endsWith(QLatin1String("/cover"))) {
        const QString idText = path.mid(14, path.length() - 14 - 6);
        bool ok = false;
        const qlonglong playlistId = idText.toLongLong(&ok);
        if (!ok)
            return HttpResponse::error(400, QStringLiteral("bad_id"));
        return handlePlaylistCover(playlistId);
    }

    return HttpResponse::error(404, QStringLiteral("not_found"));
}

HttpResponse SyncServer::handlePing()
{
    QJsonObject body;
    body[QStringLiteral("serverId")] = d->serverId;
    body[QStringLiteral("name")] = QSysInfo::machineHostName();
    body[QStringLiteral("proto")] = kProtocolVersion;
    body[QStringLiteral("appVersion")] = QCoreApplication::applicationVersion();
    return HttpResponse::json(QJsonDocument(body).toJson(QJsonDocument::Compact));
}

HttpResponse SyncServer::handlePair(const HttpRequest &request)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(request.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return HttpResponse::error(400, QStringLiteral("bad_json"));

    const QJsonObject payload = doc.object();
    const QString pin = payload.value(QStringLiteral("pin")).toString();
    const QString deviceId = payload.value(QStringLiteral("deviceId")).toString();
    const QString deviceName = payload.value(QStringLiteral("deviceName")).toString();

    if (deviceId.isEmpty())
        return HttpResponse::error(400, QStringLiteral("missing_device_id"));

    if (!d->pinGuard.verify(pin)) {
        // Re-arm nothing: a burnt PIN forces the user back to the dialog, which
        // is what stops a LAN attacker from walking the 10^6 space.
        emit pinChanged(d->pinGuard.pin());
        return HttpResponse::error(403, QStringLiteral("bad_pin"));
    }
    emit pinChanged(QString());

    const QString token = generateToken();
    QSqlDatabase db = d->database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO sync_devices (device_id, name, token_hash, created_at, last_sync_at) "
        "VALUES (?, ?, ?, ?, 0) "
        "ON CONFLICT(device_id) DO UPDATE SET name = excluded.name, "
        "                                     token_hash = excluded.token_hash"));
    q.addBindValue(deviceId);
    q.addBindValue(deviceName);
    q.addBindValue(hashToken(token));
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (!q.exec())
        return HttpResponse::error(500, QStringLiteral("store_failed"));

    emit devicePaired(deviceName.isEmpty() ? deviceId : deviceName);
    requestDeviceList();

    QJsonObject body;
    body[QStringLiteral("token")] = token;
    body[QStringLiteral("serverId")] = d->serverId;
    body[QStringLiteral("serverName")] = QSysInfo::machineHostName();
    return HttpResponse::json(QJsonDocument(body).toJson(QJsonDocument::Compact));
}

HttpResponse SyncServer::handleLibrary()
{
    QSqlDatabase db = d->database();
    const QJsonObject snapshot = buildLibrarySnapshot(db, d->serverId);
    return HttpResponse::json(QJsonDocument(snapshot).toJson(QJsonDocument::Compact));
}

HttpResponse SyncServer::handleTrackFile(qlonglong trackId, const HttpRequest &request)
{
    QSqlDatabase db = d->database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT file_path FROM tracks WHERE id = ?"));
    q.addBindValue(trackId);
    if (!q.exec() || !q.next())
        return HttpResponse::error(404, QStringLiteral("no_such_track"));

    const QString path = q.value(0).toString();
    const QFileInfo info(path);
    if (path.isEmpty() || !info.exists() || !info.isFile())
        return HttpResponse::error(404, QStringLiteral("file_missing"));

    HttpResponse response = HttpResponse::file(path, QStringLiteral("application/octet-stream"));
    response.extraHeaders.insert(QStringLiteral("X-Lumen-Size"), QString::number(info.size()));
    response.extraHeaders.insert(QStringLiteral("X-Lumen-Mtime"),
                                 QString::number(info.lastModified().toSecsSinceEpoch()));
    Q_UNUSED(request);
    return response;
}

HttpResponse SyncServer::handlePlaylistCover(qlonglong playlistId)
{
    QSqlDatabase db = d->database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT cover_image FROM playlists WHERE id = ?"));
    q.addBindValue(playlistId);
    if (!q.exec() || !q.next())
        return HttpResponse::error(404, QStringLiteral("no_such_playlist"));

    const QString path = q.value(0).toString();
    // No image is not an error: the phone falls back to the gradient, exactly
    // like the desktop's cover hierarchy.
    if (path.isEmpty() || !QFileInfo::exists(path))
        return HttpResponse::error(404, QStringLiteral("no_cover"));

    return HttpResponse::file(path, QStringLiteral("image/*"));
}

HttpResponse SyncServer::handlePush(const HttpRequest &request, const QString &deviceId)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(request.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return HttpResponse::error(400, QStringLiteral("bad_json"));

    QSqlDatabase db = d->database();
    const MergeResult result = applyPush(db, deviceId, doc.object());
    if (!result.ok)
        return HttpResponse::error(500, QStringLiteral("merge_failed"));

    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("UPDATE sync_devices SET last_sync_at = ? WHERE device_id = ?"));
        q.addBindValue(QDateTime::currentMSecsSinceEpoch());
        q.addBindValue(deviceId);
        q.exec();
    }

    if (result.changed) {
        // Queued across threads: the UI reloads its views on its own thread.
        emit libraryChangedExternally();
    }
    requestDeviceList();

    return HttpResponse::json(QJsonDocument(result.response).toJson(QJsonDocument::Compact));
}

} // namespace lumen::sync
