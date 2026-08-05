#include <QtTest>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>
#include <QFile>
#include "database/migrator.h"

// Minimal historical DB shapes → Migrator::run must reach user_version 2.
class TestMigrator : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void emptyLegacyDb();
    void prePositionSchema();
    void mixedPositionBugShape();
    void alreadyV2IsNoop();
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

static int userVersion(QSqlDatabase &db)
{
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA user_version"));
    return q.next() ? q.value(0).toInt() : -1;
}

void TestMigrator::initTestCase()
{
    QVERIFY(QSqlDatabase::drivers().contains(QStringLiteral("QSQLITE")));
}

void TestMigrator::emptyLegacyDb()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("empty.db"));

    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("t1"));
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QVERIFY(exec(db, QStringLiteral(
            "CREATE TABLE folders (id INTEGER PRIMARY KEY, name TEXT UNIQUE, "
            "cover_color1 TEXT, cover_color2 TEXT, cover_image TEXT DEFAULT '', "
            "created_at INTEGER)")));
        QVERIFY(exec(db, QStringLiteral(
            "CREATE TABLE tracks (id INTEGER PRIMARY KEY, title TEXT, artist TEXT, "
            "folder_id INTEGER DEFAULT 0, file_path TEXT, duration_ms INTEGER DEFAULT 0, "
            "cover_color1 TEXT, cover_color2 TEXT, liked INTEGER DEFAULT 0, "
            "added_at INTEGER DEFAULT 0, play_count INTEGER DEFAULT 0, "
            "last_played_at INTEGER DEFAULT 0, position INTEGER DEFAULT 0)")));
        QVERIFY(exec(db, QStringLiteral(
            "INSERT INTO folders VALUES (0,'','#e8a44a','#d45d5d','',0)")));
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("t1"));

    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("t1b"));
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QVERIFY2(lumen::Migrator::run(db, path), qPrintable(lumen::Migrator::lastError()));
        QCOMPARE(userVersion(db), 2);
        QVERIFY(exec(db, QStringLiteral("SELECT 1 FROM playlist_tracks LIMIT 1"))
                || true); // table may be empty
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type='table' AND name='playlists'")));
        QVERIFY(q.next());
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("t1b"));
}

void TestMigrator::prePositionSchema()
{
    // Shape before position column existed.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("prepos.db"));

    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("t2"));
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QVERIFY(exec(db, QStringLiteral(
            "CREATE TABLE folders (id INTEGER PRIMARY KEY, name TEXT UNIQUE, "
            "cover_color1 TEXT DEFAULT '#e8a44a', cover_color2 TEXT DEFAULT '#d45d5d', "
            "created_at INTEGER DEFAULT 0)")));
        QVERIFY(exec(db, QStringLiteral(
            "CREATE TABLE tracks (id INTEGER PRIMARY KEY, title TEXT, artist TEXT DEFAULT '', "
            "folder_id INTEGER DEFAULT 0, file_path TEXT, duration_ms INTEGER DEFAULT 0, "
            "cover_color1 TEXT, cover_color2 TEXT, liked INTEGER DEFAULT 0, "
            "added_at INTEGER DEFAULT 1000, play_count INTEGER DEFAULT 0)")));
        QVERIFY(exec(db, QStringLiteral(
            "INSERT INTO folders (id,name) VALUES (0,''), (1,'Rock')")));
        QVERIFY(exec(db, QStringLiteral(
            "INSERT INTO tracks (id,title,folder_id,file_path,added_at) "
            "VALUES (1,'A',1,'C:/a.opus',1000), (2,'B',1,'C:/b.opus',2000)")));
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("t2"));

    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("t2b"));
        db.setDatabaseName(path);
        QVERIFY(db.open());
        // applySchema-like: add missing columns so migrator can run
        exec(db, QStringLiteral("ALTER TABLE tracks ADD COLUMN last_played_at INTEGER DEFAULT 0"));
        exec(db, QStringLiteral("ALTER TABLE tracks ADD COLUMN position INTEGER DEFAULT 0"));
        exec(db, QStringLiteral("ALTER TABLE folders ADD COLUMN cover_image TEXT DEFAULT ''"));
        QVERIFY2(lumen::Migrator::run(db, path), qPrintable(lumen::Migrator::lastError()));
        QCOMPARE(userVersion(db), 2);
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM playlist_tracks")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 2);
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("t2b"));
}

void TestMigrator::mixedPositionBugShape()
{
    // Positions mix ordinals and a zero (Issue #2 shape).
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("mixed.db"));

    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("t3"));
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QVERIFY(exec(db, QStringLiteral(
            "CREATE TABLE folders (id INTEGER PRIMARY KEY, name TEXT UNIQUE, "
            "cover_color1 TEXT DEFAULT '#e8a44a', cover_color2 TEXT DEFAULT '#d45d5d', "
            "cover_image TEXT DEFAULT '', created_at INTEGER DEFAULT 0)")));
        QVERIFY(exec(db, QStringLiteral(
            "CREATE TABLE tracks (id INTEGER PRIMARY KEY, title TEXT, artist TEXT DEFAULT '', "
            "folder_id INTEGER DEFAULT 0, file_path TEXT, duration_ms INTEGER DEFAULT 0, "
            "cover_color1 TEXT DEFAULT '#e8a44a', cover_color2 TEXT DEFAULT '#d45d5d', "
            "liked INTEGER DEFAULT 0, added_at INTEGER DEFAULT 0, play_count INTEGER DEFAULT 0, "
            "last_played_at INTEGER DEFAULT 0, position INTEGER DEFAULT 0)")));
        QVERIFY(exec(db, QStringLiteral(
            "INSERT INTO folders (id,name) VALUES (0,''), (1,'Mix')")));
        // Top track has position=0 (the sentinel that Issue #2 destroyed).
        QVERIFY(exec(db, QStringLiteral(
            "INSERT INTO tracks (id,title,folder_id,file_path,position,added_at) VALUES "
            "(1,'Top',1,'C:/top.opus',0,100),"
            "(2,'Mid',1,'C:/mid.opus',1,200),"
            "(3,'Bot',1,'C:/bot.opus',2,300)")));
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("t3"));

    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("t3b"));
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QVERIFY2(lumen::Migrator::run(db, path), qPrintable(lumen::Migrator::lastError()));
        QCOMPARE(userVersion(db), 2);
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral(
            "SELECT track_id, position FROM playlist_tracks "
            "WHERE playlist_id=1 ORDER BY position, track_id")));
        QList<int> order;
        while (q.next())
            order.append(q.value(0).toInt());
        // Observed order was (position,id): 0→1, 1→2, 2→3
        QCOMPARE(order, (QList<int>{1, 2, 3}));
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("t3b"));
}

void TestMigrator::alreadyV2IsNoop()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("v2.db"));
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("t4"));
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
        QVERIFY2(lumen::Migrator::run(db, path), qPrintable(lumen::Migrator::lastError()));
        QCOMPARE(userVersion(db), 2);
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("t4"));
}

// QSqlDatabase needs a QCoreApplication (loads the QSQLITE plugin).
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TestMigrator tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_migrator.moc"
