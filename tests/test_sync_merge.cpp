#include <QtTest>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "database/migrator.h"
#include "sync/merge_service.h"

using namespace lumen::sync;

// The merge is the only path where the phone writes into the desktop's library,
// so every rule from the protocol gets a test: last-write-wins likes, additive
// play counts, idempotent playlist adoption and ownership by origin.
class TestSyncMerge : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();

    void likeAppliesWhenNewer();
    void likeIsIgnoredWhenOlder();
    void unlikeTravelsToo();
    void playCountsAreAdditive();
    void unknownTrackIsSkippedNotFatal();
    void newPlaylistIsAdoptedWithMembership();
    void repeatedPushIsIdempotent();
    void nameCollisionGetsSuffix();
    void membershipOnlyForOwnedPlaylists();

private:
    QTemporaryDir m_dir;
    QString       m_path;
    QSqlDatabase  db() { return QSqlDatabase::database(QStringLiteral("merge")); }
    static bool exec(QSqlDatabase d, const QString &sql);
    static QVariant one(QSqlDatabase d, const QString &sql);
    void seedLibrary(QSqlDatabase d);
};

bool TestSyncMerge::exec(QSqlDatabase d, const QString &sql)
{
    QSqlQuery q(d);
    if (!q.exec(sql)) {
        qWarning() << q.lastError() << sql;
        return false;
    }
    return true;
}

QVariant TestSyncMerge::one(QSqlDatabase d, const QString &sql)
{
    QSqlQuery q(d);
    if (!q.exec(sql) || !q.next())
        return {};
    return q.value(0);
}

void TestSyncMerge::seedLibrary(QSqlDatabase d)
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

void TestSyncMerge::initTestCase()
{
    QVERIFY(QSqlDatabase::drivers().contains(QStringLiteral("QSQLITE")));
    QVERIFY(m_dir.isValid());
}

void TestSyncMerge::init()
{
    m_path = m_dir.filePath(QStringLiteral("merge-%1.db").arg(QDateTime::currentMSecsSinceEpoch()));
    auto d = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("merge"));
    d.setDatabaseName(m_path);
    QVERIFY(d.open());
    seedLibrary(d);
    QVERIFY2(lumen::Migrator::run(d, m_path), qPrintable(lumen::Migrator::lastError()));

    // Three tracks to work with.
    QVERIFY(exec(d, QStringLiteral(
        "INSERT INTO tracks (title, artist, liked, liked_at, play_count, last_played_at) VALUES "
        "('A','Art',0,0,0,0), ('B','Art',0,0,5,100), ('C','Art',1,1000,0,0)")));
}

void TestSyncMerge::cleanup()
{
    {
        auto d = db();
        if (d.isOpen()) d.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("merge"));
}

void TestSyncMerge::likeAppliesWhenNewer()
{
    auto d = db();
    QJsonObject like;
    like[QStringLiteral("trackId")] = 1;
    like[QStringLiteral("liked")] = true;
    like[QStringLiteral("likedAt")] = 5000;

    QJsonObject payload;
    payload[QStringLiteral("likes")] = QJsonArray{like};

    const MergeResult result = applyPush(d, QStringLiteral("dev-1"), payload);
    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY(result.changed);

    QCOMPARE(one(d, QStringLiteral("SELECT liked FROM tracks WHERE id=1")).toInt(), 1);
    QCOMPARE(one(d, QStringLiteral("SELECT liked_at FROM tracks WHERE id=1")).toLongLong(), 5000);
}

void TestSyncMerge::likeIsIgnoredWhenOlder()
{
    auto d = db();
    // Track 3 was liked at 1000 on the desktop; a stale phone event must lose.
    QJsonObject like;
    like[QStringLiteral("trackId")] = 3;
    like[QStringLiteral("liked")] = false;
    like[QStringLiteral("likedAt")] = 500;

    QJsonObject payload;
    payload[QStringLiteral("likes")] = QJsonArray{like};

    const MergeResult result = applyPush(d, QStringLiteral("dev-1"), payload);
    QVERIFY(result.ok);
    QCOMPARE(one(d, QStringLiteral("SELECT liked FROM tracks WHERE id=3")).toInt(), 1);
}

void TestSyncMerge::unlikeTravelsToo()
{
    auto d = db();
    // Last-write-wins covers unlike without needing a tombstone.
    QJsonObject like;
    like[QStringLiteral("trackId")] = 3;
    like[QStringLiteral("liked")] = false;
    like[QStringLiteral("likedAt")] = 9000;

    QJsonObject payload;
    payload[QStringLiteral("likes")] = QJsonArray{like};

    QVERIFY(applyPush(d, QStringLiteral("dev-1"), payload).ok);
    QCOMPARE(one(d, QStringLiteral("SELECT liked FROM tracks WHERE id=3")).toInt(), 0);
}

void TestSyncMerge::playCountsAreAdditive()
{
    auto d = db();
    QJsonObject play;
    play[QStringLiteral("trackId")] = 2;   // desktop already has 5
    play[QStringLiteral("delta")] = 3;
    play[QStringLiteral("lastPlayedAt")] = 900;

    QJsonObject payload;
    payload[QStringLiteral("playCounts")] = QJsonArray{play};

    QVERIFY(applyPush(d, QStringLiteral("dev-1"), payload).ok);
    QCOMPARE(one(d, QStringLiteral("SELECT play_count FROM tracks WHERE id=2")).toInt(), 8);
    QCOMPARE(one(d, QStringLiteral("SELECT last_played_at FROM tracks WHERE id=2")).toLongLong(), 900);

    // An older timestamp must not walk the clock backwards.
    QJsonObject older;
    older[QStringLiteral("trackId")] = 2;
    older[QStringLiteral("delta")] = 1;
    older[QStringLiteral("lastPlayedAt")] = 50;
    QJsonObject second;
    second[QStringLiteral("playCounts")] = QJsonArray{older};
    QVERIFY(applyPush(d, QStringLiteral("dev-1"), second).ok);

    QCOMPARE(one(d, QStringLiteral("SELECT play_count FROM tracks WHERE id=2")).toInt(), 9);
    QCOMPARE(one(d, QStringLiteral("SELECT last_played_at FROM tracks WHERE id=2")).toLongLong(), 900);
}

void TestSyncMerge::unknownTrackIsSkippedNotFatal()
{
    auto d = db();
    QJsonObject like;
    like[QStringLiteral("trackId")] = 9999;  // deleted on the desktop meanwhile
    like[QStringLiteral("liked")] = true;
    like[QStringLiteral("likedAt")] = 5000;

    QJsonObject payload;
    payload[QStringLiteral("likes")] = QJsonArray{like};

    const MergeResult result = applyPush(d, QStringLiteral("dev-1"), payload);
    QVERIFY(result.ok);  // one stale id must not fail the whole push
    QCOMPARE(result.response.value(QStringLiteral("skipped")).toArray().size(), 1);
}

void TestSyncMerge::newPlaylistIsAdoptedWithMembership()
{
    auto d = db();
    QJsonObject playlist;
    playlist[QStringLiteral("clientKey")] = QStringLiteral("uuid-1");
    playlist[QStringLiteral("name")] = QStringLiteral("Do celular");
    playlist[QStringLiteral("coverColor1")] = QStringLiteral("#111111");
    playlist[QStringLiteral("coverColor2")] = QStringLiteral("#222222");
    playlist[QStringLiteral("trackIds")] = QJsonArray{2, 1};

    QJsonObject payload;
    payload[QStringLiteral("newPlaylists")] = QJsonArray{playlist};

    const MergeResult result = applyPush(d, QStringLiteral("dev-1"), payload);
    QVERIFY2(result.ok, qPrintable(result.error));

    QCOMPARE(one(d, QStringLiteral("SELECT COUNT(*) FROM playlists")).toInt(), 1);
    QCOMPARE(one(d, QStringLiteral("SELECT origin_device FROM playlists")).toString(),
             QStringLiteral("dev-1"));

    // Order is preserved as gap-1024 positions.
    QSqlQuery q(d);
    QVERIFY(q.exec(QStringLiteral(
        "SELECT track_id, position FROM playlist_tracks ORDER BY position")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 2);
    QCOMPARE(q.value(1).toInt(), 1024);
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);
    QCOMPARE(q.value(1).toInt(), 2048);

    const QJsonArray created = result.response.value(QStringLiteral("createdPlaylists")).toArray();
    QCOMPARE(created.size(), 1);
    QCOMPARE(created.at(0).toObject().value(QStringLiteral("clientKey")).toString(),
             QStringLiteral("uuid-1"));
}

void TestSyncMerge::repeatedPushIsIdempotent()
{
    auto d = db();
    QJsonObject playlist;
    playlist[QStringLiteral("clientKey")] = QStringLiteral("uuid-1");
    playlist[QStringLiteral("name")] = QStringLiteral("Do celular");
    playlist[QStringLiteral("trackIds")] = QJsonArray{1};

    QJsonObject payload;
    payload[QStringLiteral("newPlaylists")] = QJsonArray{playlist};

    QVERIFY(applyPush(d, QStringLiteral("dev-1"), payload).ok);
    const int firstId = one(d, QStringLiteral("SELECT id FROM playlists")).toInt();

    // The phone retries after a dropped response: the same clientKey must adopt
    // the same playlist instead of creating a second one.
    const MergeResult second = applyPush(d, QStringLiteral("dev-1"), payload);
    QVERIFY(second.ok);
    QCOMPARE(one(d, QStringLiteral("SELECT COUNT(*) FROM playlists")).toInt(), 1);
    QCOMPARE(second.response.value(QStringLiteral("createdPlaylists")).toArray()
                 .at(0).toObject().value(QStringLiteral("id")).toInt(),
             firstId);
}

void TestSyncMerge::nameCollisionGetsSuffix()
{
    auto d = db();
    QVERIFY(exec(d, QStringLiteral(
        "INSERT INTO playlists (name, cover_color1, cover_color2, created_at) "
        "VALUES ('Rock', '#1', '#2', 1)")));

    QJsonObject playlist;
    playlist[QStringLiteral("clientKey")] = QStringLiteral("uuid-rock");
    playlist[QStringLiteral("name")] = QStringLiteral("Rock");
    playlist[QStringLiteral("trackIds")] = QJsonArray{1};

    QJsonObject payload;
    payload[QStringLiteral("newPlaylists")] = QJsonArray{playlist};

    // playlists.name is UNIQUE: without the suffix the whole merge would abort.
    QVERIFY(applyPush(d, QStringLiteral("dev-1"), payload).ok);
    QCOMPARE(one(d, QStringLiteral("SELECT COUNT(*) FROM playlists")).toInt(), 2);
    QCOMPARE(one(d, QStringLiteral(
                     "SELECT name FROM playlists WHERE origin_device='dev-1'")).toString(),
             QStringLiteral("Rock (celular)"));
}

void TestSyncMerge::membershipOnlyForOwnedPlaylists()
{
    auto d = db();
    // A playlist born on the desktop.
    QVERIFY(exec(d, QStringLiteral(
        "INSERT INTO playlists (name, cover_color1, cover_color2, created_at) "
        "VALUES ('Desktop', '#1', '#2', 1)")));
    QVERIFY(exec(d, QStringLiteral(
        "INSERT INTO playlist_tracks (playlist_id, track_id, position, added_at) "
        "VALUES (1, 1, 1024, 1)")));

    QJsonObject membership;
    membership[QStringLiteral("playlistId")] = 1;
    membership[QStringLiteral("trackIds")] = QJsonArray{2, 3};

    QJsonObject payload;
    payload[QStringLiteral("playlistMembership")] = QJsonArray{membership};

    QVERIFY(applyPush(d, QStringLiteral("dev-1"), payload).ok);

    // Ownership by origin: the phone cannot rewrite a desktop-born playlist.
    QCOMPARE(one(d, QStringLiteral("SELECT COUNT(*) FROM playlist_tracks")).toInt(), 1);
    QCOMPARE(one(d, QStringLiteral("SELECT track_id FROM playlist_tracks")).toInt(), 1);
}

// QSqlDatabase needs a QCoreApplication (loads the QSQLITE plugin).
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TestSyncMerge tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_sync_merge.moc"
