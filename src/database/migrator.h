#ifndef LUMEN_MIGRATOR_H
#define LUMEN_MIGRATOR_H

#include <QSqlDatabase>
#include <QString>

// Versioned schema migrations. user_version history:
//   0 → legacy v1.x (never set a version)
//   1 → position normalised to a single ordinal scale with gap 1024  [P0b]
//   2 → N:N playlist_tracks schema                                  [P2]
//
// On failure the runner refuses to continue: a half-migrated library is worse
// than an app that will not open. See ContextProject.md §7.9–7.10.
namespace lumen {

class Migrator {
public:
    static constexpr int kSchemaVersion = 2;

    static bool run(QSqlDatabase &db, const QString &dbPath);

    static QString lastError();
    static QString lastBackupPath();

private:
    static int  pragmaInt(QSqlDatabase &db, const char *pragma);
    static bool execSql(QSqlDatabase &db, const QString &sql);
    static bool checkpointWal(QSqlDatabase &db);
    static bool backupTo(const QString &dbPath, const QString &backupPath);
    static bool tableExists(QSqlDatabase &db, const QString &name);
    static bool columnExists(QSqlDatabase &db, const QString &table, const QString &column);
    static bool recordMigration(QSqlDatabase &db, int version);

    static bool migrateTo1(QSqlDatabase &db);  // position ordinal fix
    static bool migrateTo2(QSqlDatabase &db);  // N:N playlists
};

} // namespace lumen

#endif // LUMEN_MIGRATOR_H
