#include <QtTest>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTcpSocket>
#include <QTemporaryDir>

#include "database/migrator.h"
#include "sync/library_snapshot.h"   // kProtocolVersion
#include "sync/sync_server.h"

using namespace lumen::sync;

// End-to-end run of the whole server: pair with a PIN, pull the library, fetch
// a track file (whole and resumed), push likes back. This is the automated
// version of the "curl against a running desktop" check — repeatable, and it
// does not need the GUI.
class TestSyncServer : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void pairRejectsWrongPin();
    void unauthenticatedRequestIsRejected();
    void fullJourney();

private:
    QByteArray request(const QByteArray &raw);
    QByteArray body(const QByteArray &response);
    int        statusOf(const QByteArray &response);

    QTemporaryDir m_dir;
    QString       m_dbPath;
    QString       m_audioPath;
    SyncServer   *m_server = nullptr;
    quint16       m_port = 0;
    QString       m_token;
};

void TestSyncServer::initTestCase()
{
    QVERIFY(QSqlDatabase::drivers().contains(QStringLiteral("QSQLITE")));
    QVERIFY(m_dir.isValid());

    m_dbPath = m_dir.filePath(QStringLiteral("vinil.db"));
    m_audioPath = m_dir.filePath(QStringLiteral("faixa.opus"));

    // A real file on disk so the file endpoint has something to stream.
    {
        QFile audio(m_audioPath);
        QVERIFY(audio.open(QIODevice::WriteOnly));
        audio.write(QByteArray("LUMEN").repeated(1000));  // 5000 bytes
        audio.close();
    }

    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("seed"));
        db.setDatabaseName(m_dbPath);
        QVERIFY(db.open());

        auto exec = [&db](const QString &sql) {
            QSqlQuery q(db);
            if (!q.exec(sql)) {
                qWarning() << q.lastError() << sql;
                return false;
            }
            return true;
        };

        QVERIFY(exec(QStringLiteral(
            "CREATE TABLE playlists (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE, "
            "cover_color1 TEXT, cover_color2 TEXT, cover_image TEXT DEFAULT '', "
            "dir_name TEXT DEFAULT '', sort_mode TEXT DEFAULT 'custom', created_at INTEGER)")));
        QVERIFY(exec(QStringLiteral(
            "CREATE TABLE tracks (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, artist TEXT, "
            "file_path TEXT, owner_playlist_id INTEGER, duration_ms INTEGER DEFAULT 0, "
            "cover_color1 TEXT, cover_color2 TEXT, liked INTEGER DEFAULT 0, "
            "liked_at INTEGER DEFAULT 0, added_at INTEGER DEFAULT 0, play_count INTEGER DEFAULT 0, "
            "last_played_at INTEGER DEFAULT 0, missing INTEGER DEFAULT 0)")));
        QVERIFY(exec(QStringLiteral(
            "CREATE TABLE playlist_tracks (playlist_id INTEGER, track_id INTEGER, "
            "position INTEGER, added_at INTEGER, PRIMARY KEY (playlist_id, track_id))")));
        QVERIFY(exec(QStringLiteral(
            "CREATE TABLE playback_state (id INTEGER PRIMARY KEY CHECK(id=1), "
            "current_track_id INTEGER, position_ms INTEGER, volume REAL DEFAULT 1.0, "
            "shuffle INTEGER DEFAULT 0, repeat_mode INTEGER DEFAULT 0, muted INTEGER DEFAULT 0, "
            "context_ids TEXT DEFAULT '', user_queue_ids TEXT DEFAULT '', "
            "context_index INTEGER DEFAULT -1, context_name TEXT DEFAULT '')")));
        QVERIFY(exec(QStringLiteral("PRAGMA user_version = 2")));
        QVERIFY2(lumen::Migrator::run(db, m_dbPath), qPrintable(lumen::Migrator::lastError()));

        QSqlQuery insert(db);
        insert.prepare(QStringLiteral(
            "INSERT INTO tracks (title, artist, file_path, duration_ms) VALUES (?, ?, ?, ?)"));
        insert.addBindValue(QStringLiteral("Faixa"));
        insert.addBindValue(QStringLiteral("Artista"));
        insert.addBindValue(m_audioPath);
        insert.addBindValue(215000);
        QVERIFY(insert.exec());

        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("seed"));

    // Runs on this thread: the production app puts it on a QThread, but the
    // routing logic is identical and this keeps the test deterministic.
    m_server = new SyncServer(this);
    QSignalSpy startedSpy(m_server, &SyncServer::started);
    m_server->startServer(m_dbPath, 0);   // 0 = any free port
    QVERIFY2(startedSpy.count() == 1, "server did not start");
    m_port = startedSpy.at(0).at(2).value<quint16>();
    QVERIFY(m_port != 0);
}

void TestSyncServer::cleanupTestCase()
{
    if (m_server) {
        m_server->stopServer();
        delete m_server;
        m_server = nullptr;
    }
}

QByteArray TestSyncServer::request(const QByteArray &raw)
{
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, m_port);

    QElapsedTimer timer;
    timer.start();
    while (socket.state() != QAbstractSocket::ConnectedState && timer.elapsed() < 5000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    if (socket.state() != QAbstractSocket::ConnectedState)
        return {};

    socket.write(raw);
    socket.flush();

    QByteArray response;
    timer.restart();
    while (socket.state() != QAbstractSocket::UnconnectedState && timer.elapsed() < 8000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        response.append(socket.readAll());
    }
    response.append(socket.readAll());
    return response;
}

QByteArray TestSyncServer::body(const QByteArray &response)
{
    const int start = response.indexOf("\r\n\r\n");
    return start < 0 ? QByteArray() : response.mid(start + 4);
}

int TestSyncServer::statusOf(const QByteArray &response)
{
    const QList<QByteArray> parts = response.left(response.indexOf('\r')).split(' ');
    return parts.size() > 1 ? parts.at(1).toInt() : 0;
}

void TestSyncServer::pairRejectsWrongPin()
{
    m_server->armPairing();

    const QByteArray payload = "{\"pin\":\"000000\",\"deviceId\":\"dev-x\",\"deviceName\":\"Fake\"}";
    const QByteArray response = request(
        "POST /v1/pair HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
        "Content-Length: " + QByteArray::number(payload.size()) + "\r\n\r\n" + payload);

    QCOMPARE(statusOf(response), 403);
    QVERIFY(body(response).contains("bad_pin"));
}

void TestSyncServer::unauthenticatedRequestIsRejected()
{
    const QByteArray response = request("GET /v1/library HTTP/1.1\r\nHost: x\r\n\r\n");
    QCOMPARE(statusOf(response), 401);

    // ping is the one open endpoint: the phone needs it to check reachability.
    const QByteArray ping = request("GET /v1/ping HTTP/1.1\r\nHost: x\r\n\r\n");
    QCOMPARE(statusOf(ping), 200);
    QVERIFY(body(ping).contains("serverId"));
}

void TestSyncServer::fullJourney()
{
    // --- 1. pair with the PIN the desktop is showing ---
    QSignalSpy pinSpy(m_server, &SyncServer::pinChanged);
    m_server->armPairing();
    QVERIFY(pinSpy.count() >= 1);
    const QString pin = pinSpy.last().at(0).toString();
    QCOMPARE(pin.size(), 6);

    const QByteArray pairPayload =
        QStringLiteral("{\"pin\":\"%1\",\"deviceId\":\"dev-1\",\"deviceName\":\"Celular\"}")
            .arg(pin).toUtf8();
    const QByteArray pairResponse = request(
        "POST /v1/pair HTTP/1.1\r\nHost: x\r\nContent-Length: "
        + QByteArray::number(pairPayload.size()) + "\r\n\r\n" + pairPayload);

    QCOMPARE(statusOf(pairResponse), 200);
    const QJsonObject pairJson = QJsonDocument::fromJson(body(pairResponse)).object();
    m_token = pairJson.value(QStringLiteral("token")).toString();
    QCOMPARE(m_token.size(), 64);

    const QByteArray auth = "X-Lumen-Token: " + m_token.toUtf8() + "\r\n";

    // --- 2. pull the library ---
    const QByteArray libraryResponse =
        request("GET /v1/library HTTP/1.1\r\nHost: x\r\n" + auth + "\r\n");
    QCOMPARE(statusOf(libraryResponse), 200);

    const QJsonObject snapshot = QJsonDocument::fromJson(body(libraryResponse)).object();
    QCOMPARE(snapshot.value(QStringLiteral("proto")).toInt(), kProtocolVersion);
    const QJsonArray tracks = snapshot.value(QStringLiteral("tracks")).toArray();
    QCOMPARE(tracks.size(), 1);
    QCOMPARE(tracks.at(0).toObject().value(QStringLiteral("fileSize")).toInt(), 5000);
    QCOMPARE(tracks.at(0).toObject().value(QStringLiteral("liked")).toBool(), false);

    // --- 3. download the file, whole and then resumed ---
    const QByteArray fileResponse =
        request("GET /v1/tracks/1/file HTTP/1.1\r\nHost: x\r\n" + auth + "\r\n");
    QCOMPARE(statusOf(fileResponse), 200);
    QCOMPARE(body(fileResponse).size(), 5000);
    QVERIFY(body(fileResponse).startsWith("LUMEN"));

    const QByteArray resumed = request(
        "GET /v1/tracks/1/file HTTP/1.1\r\nHost: x\r\n" + auth + "Range: bytes=4000-\r\n\r\n");
    QCOMPARE(statusOf(resumed), 206);
    QCOMPARE(body(resumed).size(), 1000);

    // --- 4. push a like back and see the library reflect it ---
    QSignalSpy changedSpy(m_server, &SyncServer::libraryChangedExternally);
    const QByteArray pushPayload =
        "{\"likes\":[{\"trackId\":1,\"liked\":true,\"likedAt\":9999}],"
        "\"playCounts\":[{\"trackId\":1,\"delta\":2,\"lastPlayedAt\":9999}]}";
    const QByteArray pushResponse = request(
        "POST /v1/push HTTP/1.1\r\nHost: x\r\n" + auth
        + "Content-Length: " + QByteArray::number(pushPayload.size()) + "\r\n\r\n" + pushPayload);

    QCOMPARE(statusOf(pushResponse), 200);
    QCOMPARE(changedSpy.count(), 1);   // the UI must be told to reload

    const QByteArray afterPush =
        request("GET /v1/library HTTP/1.1\r\nHost: x\r\n" + auth + "\r\n");
    const QJsonObject after = QJsonDocument::fromJson(body(afterPush)).object();
    const QJsonObject track = after.value(QStringLiteral("tracks")).toArray().at(0).toObject();
    QCOMPARE(track.value(QStringLiteral("liked")).toBool(), true);
    QCOMPARE(track.value(QStringLiteral("playCount")).toInt(), 2);

    // --- 5. revoking the device invalidates its token ---
    m_server->revokeDevice(QStringLiteral("dev-1"));
    const QByteArray afterRevoke =
        request("GET /v1/library HTTP/1.1\r\nHost: x\r\n" + auth + "\r\n");
    QCOMPARE(statusOf(afterRevoke), 401);
}

// QSqlDatabase needs a QCoreApplication (loads the QSQLITE plugin).
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TestSyncServer tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_sync_server.moc"
