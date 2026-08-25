#include <QtTest>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "database/migrator.h"
#include "sync/library_snapshot.h"

using namespace lumen::sync;

// Builds a modern library through the real migrator, so the snapshot is tested
// against the schema the app actually ships.
class TestSyncSnapshot : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();

    void migratorCreatesSyncTables();
    void serverIdIsStable();
    void snapshotCarriesPlaylistsTracksAndLinks();
    void snapshotNeverExposesFilePaths();
    void missingFileIsReportedAsMissing();
    void snapshotCarriesPlaybackState();

private:
    QTemporaryDir m_dir;
    QString       m_path;
    QSqlDatabase  db() { return QSqlDatabase::database(QStringLiteral("snap")); }
    static bool exec(QSqlDatabase d, const QString &sql);
    void seedV2Schema(QSqlDatabase d);
};

bool TestSyncSnapshot::exec(QSqlDatabase d, const QString &sql)
{
    QSqlQuery q(d);
    if (!q.exec(sql)) {
        qWarning() << q.lastError() << sql;
        return false;
    }
    return true;
}

void TestSyncSnapshot::seedV2Schema(QSqlDatabase d)
{
    QVERIFY(exec(d, QStringLiteral(
        "CREATE TABLE playlists (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE, "
        "cover_color1 TEXT, cover_color2 TEXT, cover_image TEXT DEFAULT '', "
        "dir_name TEXT DEFAULT '', sort_mode TEXT DEFAULT 'custom', created_at INTEGER)")));
    QVERIFY(exec(d, QStringLiteral(
        "CREATE TABLE tracks (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, artist TEXT, "
        "file_path TEXT, owner_playlist_id INTEGER, duration_ms INTEGER DEFAULT 0, "
        "cover_color1 TEXT, cover_color2 TEXT, liked INTEGER DEFAULT 0, liked_at INTEGER DEFAULT 0, "
        "added_at INTEGER DEFAULT 0, play_count INTEGER DEFAULT 0, "
        "last_played_at INTEGER DEFAULT 0, missing INTEGER DEFAULT 0)")));
    QVERIFY(exec(d, QStringLiteral(
        "CREATE TABLE playlist_tracks (playlist_id INTEGER, track_id INTEGER, "
        "position INTEGER, added_at INTEGER, PRIMARY KEY (playlist_id, track_id))")));
    QVERIFY(exec(d, QStringLiteral(
        "CREATE TABLE playback_state (id INTEGER PRIMARY KEY CHECK(id=1), "
        "current_track_id INTEGER, position_ms INTEGER, volume REAL DEFAULT 1.0, "
        "shuffle INTEGER DEFAULT 0, repeat_mode INTEGER DEFAULT 0, muted INTEGER DEFAULT 0, "
        "context_ids TEXT DEFAULT '', user_queue_ids TEXT DEFAULT '', "
        "context_index INTEGER DEFAULT -1, context_name TEXT DEFAULT '')")));
    QVERIFY(exec(d, QStringLiteral("PRAGMA user_version = 2")));
}

void TestSyncSnapshot::initTestCase()
{
    QVERIFY(QSqlDatabase::drivers().contains(QStringLiteral("QSQLITE")));
    QVERIFY(m_dir.isValid());
}

void TestSyncSnapshot::init()
{
    m_path = m_dir.filePath(QStringLiteral("snap-%1.db").arg(QDateTime::currentMSecsSinceEpoch()));
    auto d = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("snap"));
    d.setDatabaseName(m_path);
    QVERIFY(d.open());
    seedV2Schema(d);
    QVERIFY2(lumen::Migrator::run(d, m_path),
             qPrintable(lumen::Migrator::lastError()));
}

void TestSyncSnapshot::cleanup()
{
    {
        auto d = db();
        if (d.isOpen()) d.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("snap"));
}

void TestSyncSnapshot::migratorCreatesSyncTables()
{
    auto d = db();
    QSqlQuery q(d);
    QVERIFY(q.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type='table' AND name IN "
        "('sync_meta','sync_devices') ORDER BY name")));

    QStringList found;
    while (q.next()) found << q.value(0).toString();
    QCOMPARE(found, QStringList({QStringLiteral("sync_devices"), QStringLiteral("sync_meta")}));

    // The ownership-by-origin column must exist too.
    QVERIFY(q.exec(QStringLiteral("SELECT origin_device FROM playlists")));
}

void TestSyncSnapshot::serverIdIsStable()
{
    auto d = db();
    const QString first = serverId(d);
    QVERIFY(!first.isEmpty());
    // Same database, same identity — this is what keeps the phone paired.
    QCOMPARE(serverId(d), first);
}

void TestSyncSnapshot::snapshotCarriesPlaylistsTracksAndLinks()
{
    auto d = db();
    QVERIFY(exec(d, QStringLiteral(
        "INSERT INTO playlists (name, cover_color1, cover_color2, dir_name, sort_mode, created_at) "
        "VALUES ('Rock', '#111111', '#222222', 'Rock', 'custom', 100)")));
    QVERIFY(exec(d, QStringLiteral(
        "INSERT INTO tracks (title, artist, file_path, duration_ms, cover_color1, cover_color2, "
        "liked, liked_at, added_at, play_count) "
        "VALUES ('Faixa', 'Artista', '', 215000, '#aaa111', '#bbb222', 1, 500, 400, 7)")));
    QVERIFY(exec(d, QStringLiteral(
        "INSERT INTO playlist_tracks (playlist_id, track_id, position, added_at) "
        "VALUES (1, 1, 1024, 400)")));

    const QJsonObject snapshot = buildLibrarySnapshot(d, QStringLiteral("server-1"));

    QCOMPARE(snapshot.value(QStringLiteral("serverId")).toString(), QStringLiteral("server-1"));
    QCOMPARE(snapshot.value(QStringLiteral("proto")).toInt(), kProtocolVersion);

    const QJsonArray playlists = snapshot.value(QStringLiteral("playlists")).toArray();
    QCOMPARE(playlists.size(), 1);
    QCOMPARE(playlists.at(0).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("Rock"));
    QCOMPARE(playlists.at(0).toObject().value(QStringLiteral("hasCoverImage")).toBool(), false);

    const QJsonArray tracks = snapshot.value(QStringLiteral("tracks")).toArray();
    QCOMPARE(tracks.size(), 1);
    const QJsonObject track = tracks.at(0).toObject();
    QCOMPARE(track.value(QStringLiteral("title")).toString(), QStringLiteral("Faixa"));
    QCOMPARE(track.value(QStringLiteral("durationMs")).toInt(), 215000);
    QCOMPARE(track.value(QStringLiteral("liked")).toBool(), true);
    QCOMPARE(track.value(QStringLiteral("playCount")).toInt(), 7);

    const QJsonArray links = snapshot.value(QStringLiteral("playlistTracks")).toArray();
    QCOMPARE(links.size(), 1);
    QCOMPARE(links.at(0).toObject().value(QStringLiteral("position")).toInt(), 1024);
}

void TestSyncSnapshot::snapshotNeverExposesFilePaths()
{
    auto d = db();
    QVERIFY(exec(d, QStringLiteral(
        "INSERT INTO tracks (title, artist, file_path) "
        "VALUES ('X', 'Y', 'C:/Users/segredo/Music/x.opus')")));

    const QJsonObject snapshot = buildLibrarySnapshot(d, QStringLiteral("s"));
    const QJsonObject track = snapshot.value(QStringLiteral("tracks")).toArray().at(0).toObject();

    // The phone downloads by id; the desktop's disk layout is none of its business.
    QVERIFY(!track.contains(QStringLiteral("filePath")));
    QVERIFY(!track.contains(QStringLiteral("file_path")));
    QVERIFY(!QJsonDocument(snapshot).toJson().contains("segredo"));
}

void TestSyncSnapshot::missingFileIsReportedAsMissing()
{
    auto d = db();
    QVERIFY(exec(d, QStringLiteral(
        "INSERT INTO tracks (title, artist, file_path) "
        "VALUES ('Sumida', 'Y', 'C:/nao/existe/nunca.opus')")));

    const QJsonObject snapshot = buildLibrarySnapshot(d, QStringLiteral("s"));
    const QJsonObject track = snapshot.value(QStringLiteral("tracks")).toArray().at(0).toObject();

    QCOMPARE(track.value(QStringLiteral("missing")).toBool(), true);
    QCOMPARE(track.value(QStringLiteral("fileSize")).toInt(), 0);
}

void TestSyncSnapshot::snapshotCarriesPlaybackState()
{
    auto d = db();
    QVERIFY(exec(d, QStringLiteral(
        "INSERT INTO playback_state (id, current_track_id, position_ms, shuffle, repeat_mode, "
        "context_ids, user_queue_ids, context_index, context_name) "
        "VALUES (1, 7, 63000, 1, 2, '5,6,7', '42', 2, 'Curtidas')")));

    const QJsonObject snapshot = buildLibrarySnapshot(d, QStringLiteral("s"));
    const QJsonObject state = snapshot.value(QStringLiteral("playbackState")).toObject();

    QCOMPARE(state.value(QStringLiteral("currentTrackId")).toInt(), 7);
    QCOMPARE(state.value(QStringLiteral("positionMs")).toInt(), 63000);
    QCOMPARE(state.value(QStringLiteral("shuffle")).toBool(), true);
    QCOMPARE(state.value(QStringLiteral("repeatMode")).toInt(), 2);
    QCOMPARE(state.value(QStringLiteral("contextName")).toString(), QStringLiteral("Curtidas"));

    // Comma-joined ids on disk become a real JSON array on the wire.
    const QJsonArray context = state.value(QStringLiteral("contextIds")).toArray();
    QCOMPARE(context.size(), 3);
    QCOMPARE(context.at(2).toInt(), 7);
    QCOMPARE(state.value(QStringLiteral("userQueueIds")).toArray().at(0).toInt(), 42);
}

// QSqlDatabase needs a QCoreApplication (loads the QSQLITE plugin).
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TestSyncSnapshot tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_sync_snapshot.moc"
