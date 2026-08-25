#ifndef LUMEN_SYNC_SYNC_SERVER_H
#define LUMEN_SYNC_SYNC_SERVER_H

#include <QObject>
#include <QString>
#include <QVector>

#include "discovery_responder.h"
#include "http_server.h"
#include "pairing.h"

namespace lumen::sync {

struct PairedDevice {
    QString   deviceId;
    QString   name;
    qlonglong createdAt = 0;
    qlonglong lastSyncAt = 0;
};

// Owns the HTTP server, the discovery responder and the SQL connection used to
// serve requests.
//
// Lives on its own QThread with its own event loop: serialising the library and
// streaming a whole track must never run on the UI thread. It also opens its own
// QSqlDatabase connection (named "sync") — a QSqlDatabase handle belongs to the
// thread that created it. WAL lets that connection read while the UI writes, and
// busy_timeout covers the short merge writes.
class SyncServer : public QObject {
    Q_OBJECT
public:
    explicit SyncServer(QObject *parent = nullptr);
    ~SyncServer() override;

    bool isRunning() const { return m_running; }

public slots:
    // Called from the UI thread via queued connections.
    void startServer(const QString &dbPath, quint16 port);
    void stopServer();
    void armPairing();
    void disarmPairing();
    void revokeDevice(const QString &deviceId);
    void requestDeviceList();

signals:
    void started(const QString &serverId, const QString &address, quint16 port);
    void startFailed(const QString &reason);
    void stopped();
    void pinChanged(const QString &pin);
    void devicePaired(const QString &deviceName);
    void deviceListChanged(const QVector<lumen::sync::PairedDevice> &devices);

    // A phone's push changed the library; the UI must reload its views.
    void libraryChangedExternally();

private:
    // Routing table. Runs on the server thread, never on the UI thread.
    HttpResponse handleRequest(const HttpRequest &request);
    HttpResponse handlePing();
    HttpResponse handlePair(const HttpRequest &request);
    HttpResponse handleLibrary();
    HttpResponse handleTrackFile(qlonglong trackId, const HttpRequest &request);
    HttpResponse handlePlaylistCover(qlonglong playlistId);
    HttpResponse handlePush(const HttpRequest &request, const QString &deviceId);

    // Returns the device id behind a valid token, or an empty string.
    QString authenticate(const HttpRequest &request);

    class Impl;
    Impl *d;
    bool m_running = false;
};

} // namespace lumen::sync

Q_DECLARE_METATYPE(QVector<lumen::sync::PairedDevice>)

#endif // LUMEN_SYNC_SYNC_SERVER_H
