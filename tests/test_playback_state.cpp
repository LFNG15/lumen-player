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

// QSqlDatabase needs a QCoreApplication (loads the QSQLITE plugin).
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TestPlaybackState tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_playback_state.moc"
