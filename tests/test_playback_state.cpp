#include <QtTest>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include "database.h"

// Round-trip tests for Database::saveState / loadState (issues #20 / #21).
class TestPlaybackState : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();

    void roundTrip_allFields();
    void roundTrip_emptyLists();
    void roundTrip_volumeBounds();
    void roundTrip_overwrite();
    void legacySchemaMissingContextColumns();

private:
    QTemporaryDir m_dir;
};

void TestPlaybackState::initTestCase()
{
    QVERIFY(m_dir.isValid());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(m_dir.filePath(QStringLiteral("state.db")));
    QVERIFY(db.open());
}

void TestPlaybackState::roundTrip_allFields()
{
    Database::PlaybackState s;
    s.trackId      = 42;
    s.posMs        = 123456;
    s.volume       = 0.35;
    s.muted        = true;
    s.shuffle      = true;
    s.repeatMode   = 2;
    s.contextIds   = {1, 2, 3};
    s.userQueueIds = {9, 8};
    s.contextIndex = 1;
    s.contextName  = QStringLiteral("Curtidas");

    Database::instance().saveState(s);
    const auto loaded = Database::instance().loadState();

    QCOMPARE(loaded.trackId, 42);
    QCOMPARE(loaded.posMs, qint64(123456));
    QCOMPARE(loaded.volume, 0.35);
    QCOMPARE(loaded.muted, true);
    QCOMPARE(loaded.shuffle, true);
    QCOMPARE(loaded.repeatMode, 2);
    QCOMPARE(loaded.contextIds, QList<int>({1, 2, 3}));
    QCOMPARE(loaded.userQueueIds, QList<int>({9, 8}));
    QCOMPARE(loaded.contextIndex, 1);
    QCOMPARE(loaded.contextName, QStringLiteral("Curtidas"));
}

void TestPlaybackState::roundTrip_emptyLists()
{
    Database::PlaybackState s;
    s.trackId      = 7;
    s.posMs        = 1;
    s.volume       = 0.5;
    s.contextIds   = {};
    s.userQueueIds = {};
    s.contextIndex = -1;
    s.contextName  = QString();

    Database::instance().saveState(s);
    const auto loaded = Database::instance().loadState();

    QCOMPARE(loaded.trackId, 7);
    QVERIFY(loaded.contextIds.isEmpty());
    QVERIFY(loaded.userQueueIds.isEmpty());
    QCOMPARE(loaded.contextIndex, -1);
    QVERIFY(loaded.contextName.isEmpty());
}

void TestPlaybackState::roundTrip_volumeBounds()
{
    Database::PlaybackState s;
    s.volume = 0.0;
    Database::instance().saveState(s);
    QCOMPARE(Database::instance().loadState().volume, 0.0);

    s.volume = 1.0;
    Database::instance().saveState(s);
    QCOMPARE(Database::instance().loadState().volume, 1.0);
}

void TestPlaybackState::roundTrip_overwrite()
{
    Database::PlaybackState first;
    first.trackId = 1;
    first.volume  = 0.2;
    first.contextIds = {1};
    Database::instance().saveState(first);

    Database::PlaybackState second;
    second.trackId = 99;
    second.posMs = 50;
    second.volume = 0.8;
    second.muted = true;
    second.shuffle = true;
    second.repeatMode = 1;
    second.contextIds = {4, 5};
    second.userQueueIds = {6};
    second.contextIndex = 0;
    second.contextName = QStringLiteral("Biblioteca");
    Database::instance().saveState(second);

    const auto loaded = Database::instance().loadState();
    QCOMPARE(loaded.trackId, 99);
    QCOMPARE(loaded.posMs, qint64(50));
    QCOMPARE(loaded.volume, 0.8);
    QCOMPARE(loaded.muted, true);
    QCOMPARE(loaded.shuffle, true);
    QCOMPARE(loaded.repeatMode, 1);
    QCOMPARE(loaded.contextIds, QList<int>({4, 5}));
    QCOMPARE(loaded.userQueueIds, QList<int>({6}));
    QCOMPARE(loaded.contextIndex, 0);
    QCOMPARE(loaded.contextName, QStringLiteral("Biblioteca"));
}

void TestPlaybackState::legacySchemaMissingContextColumns()
{
    // Shape of real v2.0.0 user DBs: old repeat CHECK + some ALTERs, but no
    // context_index / context_name. loadState used to SELECT those columns and
    // return defaults, wiping a perfectly good persist.
    QSqlQuery q(QSqlDatabase::database());
    QVERIFY(q.exec(QStringLiteral("DROP TABLE IF EXISTS playback_state")));
    QVERIFY(q.exec(QStringLiteral(
        "CREATE TABLE playback_state ("
        "  id INTEGER PRIMARY KEY CHECK (id = 1),"
        "  current_track_id INTEGER NOT NULL DEFAULT 0,"
        "  position_ms INTEGER NOT NULL DEFAULT 0,"
        "  volume REAL NOT NULL DEFAULT 0.7 CHECK (volume >= 0.0 AND volume <= 1.0),"
        "  shuffle INTEGER NOT NULL DEFAULT 0 CHECK (shuffle IN (0,1)),"
        "  repeat_mode INTEGER NOT NULL DEFAULT 0 CHECK (repeat_mode IN (0,1)),"
        "  muted INTEGER NOT NULL DEFAULT 0,"
        "  context_ids TEXT NOT NULL DEFAULT '',"
        "  user_queue_ids TEXT NOT NULL DEFAULT ''"
        ")")));
    QVERIFY(q.exec(QStringLiteral(
        "INSERT INTO playback_state "
        "(id, current_track_id, position_ms, volume, context_ids, user_queue_ids) "
        "VALUES (1, 22, 38761, 1.0, '27,28,29', '22')")));

    const auto loaded = Database::instance().loadState();
    QCOMPARE(loaded.trackId, 22);
    QCOMPARE(loaded.posMs, qint64(38761));
    QCOMPARE(loaded.volume, 1.0);
    QCOMPARE(loaded.contextIds, QList<int>({27, 28, 29}));
    QCOMPARE(loaded.userQueueIds, QList<int>({22}));

    Database::PlaybackState s = loaded;
    s.posMs = 60000;
    s.volume = 1.0;
    s.contextName = QStringLiteral("Fila");
    Database::instance().saveState(s);
    const auto again = Database::instance().loadState();
    QCOMPARE(again.trackId, 22);
    QCOMPARE(again.posMs, qint64(60000));
    QCOMPARE(again.volume, 1.0);
    QCOMPARE(again.contextName, QStringLiteral("Fila"));
}

// QSqlDatabase needs a QCoreApplication (loads the QSQLITE plugin).
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TestPlaybackState tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_playback_state.moc"
