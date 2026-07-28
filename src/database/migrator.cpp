#include "migrator.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <QVariant>
#include <QPair>
#include <QList>

namespace lumen {

namespace {

QString g_lastError;
QString g_lastBackupPath;

bool fail(const QString &msg)
{
    g_lastError = msg;
    qWarning() << "Migrator:" << msg;
    return false;
}

} // namespace

QString Migrator::lastError()      { return g_lastError; }
QString Migrator::lastBackupPath() { return g_lastBackupPath; }

int Migrator::pragmaInt(QSqlDatabase &db, const char *pragma)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA %1").arg(QLatin1String(pragma))) || !q.next())
        return -1;
    return q.value(0).toInt();
}

bool Migrator::execSql(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        return fail(QStringLiteral("SQL failed: %1 — %2")
                        .arg(sql.left(120), q.lastError().text()));
    }
    return true;
}

bool Migrator::checkpointWal(QSqlDatabase &db)
{
    // WAL checkpoint must complete before a file-level backup, otherwise the
    // copy of vinil.db silently loses recent commits (ContextProject §7.10).
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"))) {
        return fail(QStringLiteral("wal_checkpoint failed: %1")
                        .arg(q.lastError().text()));
    }
    return true;
}

bool Migrator::backupTo(const QString &dbPath, const QString &backupPath)
{
    g_lastBackupPath = backupPath;
    if (!QFile::exists(dbPath))
        return true; // brand-new DB; nothing to back up

    if (QFile::exists(backupPath) && !QFile::remove(backupPath)) {
        return fail(QStringLiteral("could not replace previous backup at %1")
                        .arg(backupPath));
    }
    if (!QFile::copy(dbPath, backupPath)) {
        return fail(QStringLiteral("backup copy failed → %1").arg(backupPath));
    }
    // Also copy -wal/-shm if present (should be empty after checkpoint, but
    // keep them so a manual restore is complete).
    const QString wal = dbPath + QStringLiteral("-wal");
    const QString shm = dbPath + QStringLiteral("-shm");
    if (QFile::exists(wal))
        QFile::copy(wal, backupPath + QStringLiteral("-wal"));
    if (QFile::exists(shm))
        QFile::copy(shm, backupPath + QStringLiteral("-shm"));
    return true;
}

// user_version 0 → 1
// Fix Issue #2: eliminate the position=0 sentinel and unify the scale to
// ordinal gaps of 1024 (rank starts at 1 → 1024, 2048, …).
//
// Order by (position, id) — the same key tracksInFolder() uses today — so we
// migrate the *observed* order on screen, not a theoretically intended one.
bool Migrator::migrateTo1(QSqlDatabase &db)
{
    // Collect folder_ids that have tracks (including sentinel 0 / standalone).
    QSqlQuery folders(db);
    if (!folders.exec(QStringLiteral(
            "SELECT DISTINCT folder_id FROM tracks ORDER BY folder_id"))) {
        return fail(folders.lastError().text());
    }

    QList<int> folderIds;
    while (folders.next())
        folderIds.append(folders.value(0).toInt());

    QSqlQuery select(db);
    select.prepare(QStringLiteral(
        "SELECT id FROM tracks WHERE folder_id = ? ORDER BY position, id"));

    QSqlQuery update(db);
    update.prepare(QStringLiteral(
        "UPDATE tracks SET position = ? WHERE id = ?"));

    for (int folderId : folderIds) {
        select.bindValue(0, folderId);
        if (!select.exec())
            return fail(select.lastError().text());

        QList<int> ids;
        while (select.next())
            ids.append(select.value(0).toInt());

        for (int rank = 0; rank < ids.size(); ++rank) {
            // rank 0 → 1024, rank 1 → 2048, … — 0 is never a valid position.
            const qint64 pos = static_cast<qint64>(rank + 1) * 1024;
            update.bindValue(0, pos);
            update.bindValue(1, ids[rank]);
            if (!update.exec())
                return fail(update.lastError().text());
        }
    }

    // Ensure schema_migrations exists (created here so P2 can rely on it).
    if (!execSql(db, QStringLiteral(
            "CREATE TABLE IF NOT EXISTS schema_migrations ("
            "  version     INTEGER PRIMARY KEY,"
            "  applied_at  INTEGER NOT NULL,"
            "  app_version TEXT    NOT NULL"
            ")"))) {
        return false;
    }

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO schema_migrations (version, applied_at, app_version) "
        "VALUES (?, ?, ?)"));
    insert.bindValue(0, 1);
    insert.bindValue(1, QDateTime::currentMSecsSinceEpoch());
    insert.bindValue(2, QStringLiteral("2.0.0"));
    if (!insert.exec())
        return fail(insert.lastError().text());

    return true;
}

bool Migrator::run(QSqlDatabase &db, const QString &dbPath)
{
    g_lastError.clear();
    g_lastBackupPath.clear();

    int v = pragmaInt(db, "user_version");
    if (v < 0)
        return fail(QStringLiteral("could not read PRAGMA user_version"));

    if (v > kSchemaVersion) {
        return fail(QStringLiteral(
            "database was written by a newer Lumen Music (user_version=%1, "
            "this build supports %2)")
                        .arg(v)
                        .arg(kSchemaVersion));
    }
    if (v == kSchemaVersion)
        return true;

    if (!checkpointWal(db))
        return false;

    const QString backupPath =
        dbPath + QStringLiteral(".v%1.bak").arg(v);
    if (!backupTo(dbPath, backupPath))
        return false;

    // PRAGMA foreign_keys is a no-op inside a transaction — toggle outside
    // (ContextProject §7.9).
    if (!execSql(db, QStringLiteral("PRAGMA foreign_keys = OFF")))
        return false;

    if (!db.transaction())
        return fail(QStringLiteral("BEGIN failed: %1").arg(db.lastError().text()));

    // Apply migrations in order.
    if (v < 1) {
        if (!migrateTo1(db)) {
            db.rollback();
            execSql(db, QStringLiteral("PRAGMA foreign_keys = ON"));
            return fail(QStringLiteral("migration to v1 failed: %1 (backup: %2)")
                            .arg(g_lastError, backupPath));
        }
        if (!execSql(db, QStringLiteral("PRAGMA user_version = 1"))) {
            db.rollback();
            execSql(db, QStringLiteral("PRAGMA foreign_keys = ON"));
            return false;
        }
    }

    // foreign_key_check before commit.
    QSqlQuery fk(db);
    if (!fk.exec(QStringLiteral("PRAGMA foreign_key_check")) || fk.next()) {
        const QString detail = fk.isActive() && fk.isValid()
            ? fk.value(0).toString()
            : fk.lastError().text();
        db.rollback();
        execSql(db, QStringLiteral("PRAGMA foreign_keys = ON"));
        return fail(QStringLiteral("foreign_key_check failed: %1 (backup: %2)")
                        .arg(detail, backupPath));
    }

    if (!db.commit()) {
        execSql(db, QStringLiteral("PRAGMA foreign_keys = ON"));
        return fail(QStringLiteral("COMMIT failed: %1 (backup: %2)")
                        .arg(db.lastError().text(), backupPath));
    }

    if (!execSql(db, QStringLiteral("PRAGMA foreign_keys = ON")))
        return false;

    qInfo() << "Migrator: upgraded schema to user_version"
            << kSchemaVersion << "backup at" << backupPath;
    return true;
}

} // namespace lumen
