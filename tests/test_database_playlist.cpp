#include <QtTest>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>
#include <QColor>
#include <QUrl>
#include "database.h"
#include "trackmodel.h"
#include "migrator.h"

// Characterization tests for Database::addTrackToPlaylist (the "owner playlist" rule,
// decision 7) and the playlist position-gap machinery, pinned down BEFORE any refactor
// per REFATORACAO.md §2 — nothing here should change once the extraction in the plan
// (owner_decision.h, position_gap.h wiring) lands; every assertion must still hold.
class TestDatabasePlaylist : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void smokeCreatePlaylist();

    void addTrackToPlaylist_invalidIds();
    void addTrackToPlaylist_firstOwner();
    void addTrackToPlaylist_reAddAfterRemoval();
    void addTrackToPlaylist_crossesOwner();
    void addTrackToPlaylist_alreadyPresent();

    void normalisePlaylistPositions_reordersToGap();
    void nextPositionInPlaylist_emptyAndNonEmpty();
    void trackModelReorderPlaylist_matchesGap();

private:
    // Must outlive every test function — Database is a process-wide singleton bound
    // to the default SQL connection, so there is exactly one temp DB for this whole run.
    QTemporaryDir m_dir;
};

static bool exec(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning() << q.lastError() << sql;
        return false;
    }
    return true;
}

static Track makeStandaloneTrack(const QString &path)
{
    Track t;
    t.title  = QStringLiteral("Track");
    t.artist = QStringLiteral("Artist");
    t.audioUrl = QUrl::fromLocalFile(path);
    t.cover.c1 = QColor(QStringLiteral("#e8a44a"));
    t.cover.c2 = QColor(QStringLiteral("#d45d5d"));
    return t;
}

void TestDatabasePlaylist::initTestCase()
{
    QVERIFY(m_dir.isValid());
    const QString path = m_dir.filePath(QStringLiteral("test.db"));

    // Build the v2 shape directly (same DDL test_migrator.cpp's alreadyV2IsNoop() uses)
    // on the DEFAULT connection, already open. Database::open() short-circuits when it
    // finds the default connection already open — its own applySchema()/Migrator::run()
    // never runs in that case, which is exactly why we build the schema ourselves here
    // instead of relying on it. This is also the only seam that exists: Database::open()
    // otherwise always points at the real %LOCALAPPDATA%/vinil.db with no way to inject
    // a test path.
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(path);
    QVERIFY(db.open());

    QVERIFY(exec(db, QStringLiteral(
        "CREATE TABLE playlists (id INTEGER PRIMARY KEY, name TEXT UNIQUE, "
        "cover_color1 TEXT, cover_color2 TEXT, cover_image TEXT DEFAULT '', "
        "dir_name TEXT DEFAULT '', sort_mode TEXT DEFAULT 'custom', created_at INTEGER)")));
    QVERIFY(exec(db, QStringLiteral(
        "CREATE TABLE tracks (id INTEGER PRIMARY KEY, title TEXT, artist TEXT, "
        "file_path TEXT UNIQUE, owner_playlist_id INTEGER, duration_ms INTEGER, "
        "cover_color1 TEXT, cover_color2 TEXT, liked INTEGER, liked_at INTEGER, "
        "added_at INTEGER, play_count INTEGER, last_played_at INTEGER, missing INTEGER)")));
    QVERIFY(exec(db, QStringLiteral(
        "CREATE TABLE playlist_tracks (playlist_id INTEGER, track_id INTEGER, "
        "position INTEGER, added_at INTEGER, PRIMARY KEY (playlist_id, track_id)) "
        "WITHOUT ROWID")));
    QVERIFY(exec(db, QStringLiteral("PRAGMA user_version = 2")));

    // Confirms the schema above really does satisfy the migrator's "already v2" check
    // instead of silently drifting from it.
    QVERIFY2(lumen::Migrator::run(db, path), qPrintable(lumen::Migrator::lastError()));

    QVERIFY(Database::instance().open());
}

void TestDatabasePlaylist::init()
{
    // Isolation between test functions: wipe rows, keep schema. Can't use a fresh
    // connection per test (see class comment) — the singleton stays bound for the
    // whole run.
    QSqlQuery q(QSqlDatabase::database());
    QVERIFY(q.exec(QStringLiteral("DELETE FROM playlist_tracks")));
    QVERIFY(q.exec(QStringLiteral("DELETE FROM tracks")));
    QVERIFY(q.exec(QStringLiteral("DELETE FROM playlists")));
}

void TestDatabasePlaylist::cleanupTestCase()
{
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

void TestDatabasePlaylist::smokeCreatePlaylist()
{
    const int id = Database::instance().createPlaylist(
        QStringLiteral("Smoke"), QColor(QStringLiteral("#e8a44a")), QColor(QStringLiteral("#d45d5d")));
    QVERIFY(id > 0);
}

void TestDatabasePlaylist::addTrackToPlaylist_invalidIds()
{
    QCOMPARE(Database::instance().addTrackToPlaylist(0, 1).status, AddToPlaylistResult::Failed);
    QCOMPARE(Database::instance().addTrackToPlaylist(1, 0).status, AddToPlaylistResult::Failed);

    const int playlistId = Database::instance().createPlaylist(
        QStringLiteral("P"), QColor(QStringLiteral("#e8a44a")), QColor(QStringLiteral("#d45d5d")));
    QCOMPARE(Database::instance().addTrackToPlaylist(999999, playlistId).status,
              AddToPlaylistResult::Failed);
}

void TestDatabasePlaylist::addTrackToPlaylist_firstOwner()
{
    const int playlistId = Database::instance().createPlaylist(
        QStringLiteral("P1"), QColor(QStringLiteral("#e8a44a")), QColor(QStringLiteral("#d45d5d")));
    const int trackId = Database::instance().insertTrack(
        makeStandaloneTrack(QStringLiteral("C:/a.opus")), 0);
    QVERIFY(trackId > 0);

    const auto r = Database::instance().addTrackToPlaylist(trackId, playlistId);
    QCOMPARE(r.status, AddToPlaylistResult::Added);
    QVERIFY(r.firstOwner);
    QVERIFY(!r.crossesOwner);

    QSqlQuery q(QSqlDatabase::database());
    q.prepare(QStringLiteral("SELECT owner_playlist_id FROM tracks WHERE id = ?"));
    q.addBindValue(trackId);
    QVERIFY(q.exec() && q.next());
    QCOMPARE(q.value(0).toInt(), playlistId);

    QSqlQuery pos(QSqlDatabase::database());
    pos.prepare(QStringLiteral(
        "SELECT position FROM playlist_tracks WHERE playlist_id = ? AND track_id = ?"));
    pos.addBindValue(playlistId);
    pos.addBindValue(trackId);
    QVERIFY(pos.exec() && pos.next());
    QCOMPARE(pos.value(0).toLongLong(), qint64(1024));
}

void TestDatabasePlaylist::addTrackToPlaylist_reAddAfterRemoval()
{
    // The subtle branch: owner_playlist_id is advisory and is never cleared by
    // removeTrackFromPlaylist (database.cpp, "Owner is advisory... file stays where it
    // is"). Re-adding to the SAME playlist after removal must not look like a fresh
    // "first owner" — it isn't, the file never moved.
    const int playlistId = Database::instance().createPlaylist(
        QStringLiteral("P1"), QColor(QStringLiteral("#e8a44a")), QColor(QStringLiteral("#d45d5d")));
    const int trackId = Database::instance().insertTrack(
        makeStandaloneTrack(QStringLiteral("C:/re.opus")), playlistId);
    QVERIFY(trackId > 0);

    QVERIFY(Database::instance().removeTrackFromPlaylist(trackId, playlistId));

    const auto r = Database::instance().addTrackToPlaylist(trackId, playlistId);
    QCOMPARE(r.status, AddToPlaylistResult::Added);
    QVERIFY(!r.firstOwner);
    QVERIFY(!r.crossesOwner);
}

void TestDatabasePlaylist::addTrackToPlaylist_crossesOwner()
{
    // NOTE: this exercises Database::playlistDiskPath()'s QDir::mkpath side effect for
    // the owner directory (real filesystem write under the configured downloads root) —
    // that's inherent to the branch being characterized, not something this test adds.
    const int owner = Database::instance().createPlaylist(
        QStringLiteral("Owner"), QColor(QStringLiteral("#e8a44a")), QColor(QStringLiteral("#d45d5d")));
    const int other = Database::instance().createPlaylist(
        QStringLiteral("Other"), QColor(QStringLiteral("#e8a44a")), QColor(QStringLiteral("#d45d5d")));
    const int trackId = Database::instance().insertTrack(
        makeStandaloneTrack(QStringLiteral("C:/x.opus")), owner);

    const auto r = Database::instance().addTrackToPlaylist(trackId, other);
    QCOMPARE(r.status, AddToPlaylistResult::Added);
    QVERIFY(!r.firstOwner);
    QVERIFY(r.crossesOwner);
    QCOMPARE(r.ownerName, QStringLiteral("Owner"));

    QSqlQuery q(QSqlDatabase::database());
    q.prepare(QStringLiteral("SELECT owner_playlist_id FROM tracks WHERE id = ?"));
    q.addBindValue(trackId);
    QVERIFY(q.exec() && q.next());
    QCOMPARE(q.value(0).toInt(), owner); // unchanged — still the original owner
}

void TestDatabasePlaylist::addTrackToPlaylist_alreadyPresent()
{
    const int playlistId = Database::instance().createPlaylist(
        QStringLiteral("P"), QColor(QStringLiteral("#e8a44a")), QColor(QStringLiteral("#d45d5d")));
    const int trackId = Database::instance().insertTrack(
        makeStandaloneTrack(QStringLiteral("C:/dup.opus")), playlistId);

    const auto r = Database::instance().addTrackToPlaylist(trackId, playlistId);
    QCOMPARE(r.status, AddToPlaylistResult::AlreadyPresent);

    QSqlQuery q(QSqlDatabase::database());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM playlist_tracks WHERE playlist_id = ? AND track_id = ?"));
    q.addBindValue(playlistId);
    q.addBindValue(trackId);
    QVERIFY(q.exec() && q.next());
    QCOMPARE(q.value(0).toInt(), 1); // no duplicate row
}

void TestDatabasePlaylist::normalisePlaylistPositions_reordersToGap()
{
    const int playlistId = Database::instance().createPlaylist(
        QStringLiteral("P"), QColor(QStringLiteral("#e8a44a")), QColor(QStringLiteral("#d45d5d")));
    const int t1 = Database::instance().insertTrack(makeStandaloneTrack(QStringLiteral("C:/1.opus")), playlistId);
    const int t2 = Database::instance().insertTrack(makeStandaloneTrack(QStringLiteral("C:/2.opus")), playlistId);
    const int t3 = Database::instance().insertTrack(makeStandaloneTrack(QStringLiteral("C:/3.opus")), playlistId);

    // Scramble to out-of-order, non-gap positions directly, bypassing the app.
    QSqlQuery q(QSqlDatabase::database());
    auto setPos = [&](int trackId, qint64 pos) {
        q.prepare(QStringLiteral(
            "UPDATE playlist_tracks SET position = ? WHERE playlist_id = ? AND track_id = ?"));
        q.addBindValue(pos);
        q.addBindValue(playlistId);
        q.addBindValue(trackId);
        QVERIFY(q.exec());
    };
    setPos(t1, 5);
    setPos(t2, 1);
    setPos(t3, 3);

    Database::instance().normalisePlaylistPositions(playlistId);

    QSqlQuery sel(QSqlDatabase::database());
    sel.prepare(QStringLiteral(
        "SELECT track_id, position FROM playlist_tracks WHERE playlist_id = ? ORDER BY position"));
    sel.addBindValue(playlistId);
    QVERIFY(sel.exec());
    QList<QPair<int, qint64>> got;
    while (sel.next())
        got.append(qMakePair(sel.value(0).toInt(), sel.value(1).toLongLong()));

    QCOMPARE(got.size(), 3);
    QCOMPARE(got[0], qMakePair(t2, qint64(1024)));
    QCOMPARE(got[1], qMakePair(t3, qint64(2048)));
    QCOMPARE(got[2], qMakePair(t1, qint64(3072)));
}

void TestDatabasePlaylist::nextPositionInPlaylist_emptyAndNonEmpty()
{
    const int playlistId = Database::instance().createPlaylist(
        QStringLiteral("P"), QColor(QStringLiteral("#e8a44a")), QColor(QStringLiteral("#d45d5d")));
    QCOMPARE(Database::instance().nextPositionInPlaylist(playlistId), qint64(1024));

    Database::instance().insertTrack(makeStandaloneTrack(QStringLiteral("C:/n1.opus")), playlistId);
    QCOMPARE(Database::instance().nextPositionInPlaylist(playlistId), qint64(2048));
}

void TestDatabasePlaylist::trackModelReorderPlaylist_matchesGap()
{
    TrackModel model;
    const int playlistId = model.createPlaylist(
        QStringLiteral("P"), QColor(QStringLiteral("#e8a44a")), QColor(QStringLiteral("#d45d5d")));
    const int t1 = Database::instance().insertTrack(makeStandaloneTrack(QStringLiteral("C:/r1.opus")), playlistId);
    const int t2 = Database::instance().insertTrack(makeStandaloneTrack(QStringLiteral("C:/r2.opus")), playlistId);
    const int t3 = Database::instance().insertTrack(makeStandaloneTrack(QStringLiteral("C:/r3.opus")), playlistId);

    model.reorderPlaylist(playlistId, {t3, t1, t2});

    QSqlQuery sel(QSqlDatabase::database());
    sel.prepare(QStringLiteral(
        "SELECT track_id FROM playlist_tracks WHERE playlist_id = ? ORDER BY position"));
    sel.addBindValue(playlistId);
    QVERIFY(sel.exec());
    QList<int> order;
    while (sel.next())
        order.append(sel.value(0).toInt());
    QCOMPARE(order, (QList<int>{t3, t1, t2}));
}

// QSqlDatabase needs a QCoreApplication (loads the QSQLITE plugin).
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TestDatabasePlaylist tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_database_playlist.moc"
