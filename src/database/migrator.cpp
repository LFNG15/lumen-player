#include "migrator.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QDateTime>
#include <QDebug>
#include <QVariant>
#include <QList>
#include <QRegularExpression>
#include <QHash>
#include <QSet>

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

QString sanitizeDirName(QString name)
{
    static const QRegularExpression bad(QStringLiteral(R"([\\/:*?"<>|])"));
    name.replace(bad, QStringLiteral("_"));
    return name.trimmed();
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
                        .arg(sql.left(160), q.lastError().text()));
    }
    return true;
}

bool Migrator::checkpointWal(QSqlDatabase &db)
{
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
        return true;

    if (QFile::exists(backupPath) && !QFile::remove(backupPath)) {
        return fail(QStringLiteral("could not replace previous backup at %1")
                        .arg(backupPath));
    }
    if (!QFile::copy(dbPath, backupPath)) {
        return fail(QStringLiteral("backup copy failed → %1").arg(backupPath));
    }
    const QString wal = dbPath + QStringLiteral("-wal");
    const QString shm = dbPath + QStringLiteral("-shm");
    if (QFile::exists(wal))
        QFile::copy(wal, backupPath + QStringLiteral("-wal"));
    if (QFile::exists(shm))
        QFile::copy(shm, backupPath + QStringLiteral("-shm"));
    return true;
}

bool Migrator::tableExists(QSqlDatabase &db, const QString &name)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?"));
    q.addBindValue(name);
    return q.exec() && q.next();
}

bool Migrator::columnExists(QSqlDatabase &db, const QString &table, const QString &column)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)))
        return false;
    while (q.next()) {
        if (q.value(1).toString() == column)
            return true;
    }
    return false;
}

bool Migrator::recordMigration(QSqlDatabase &db, int version)
{
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
    insert.bindValue(0, version);
    insert.bindValue(1, QDateTime::currentMSecsSinceEpoch());
    insert.bindValue(2, QStringLiteral("2.0.0"));
    if (!insert.exec())
        return fail(insert.lastError().text());
    return true;
}

bool Migrator::migrateTo1(QSqlDatabase &db)
{
    // Only meaningful on the pre-N:N schema (tracks.folder_id + tracks.position).
    if (!tableExists(db, QStringLiteral("tracks")))
        return recordMigration(db, 1);

    if (!columnExists(db, QStringLiteral("tracks"), QStringLiteral("folder_id"))) {
        // Already past v1 shape; just stamp.
        return recordMigration(db, 1);
    }

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
            const qint64 pos = static_cast<qint64>(rank + 1) * 1024;
            update.bindValue(0, pos);
            update.bindValue(1, ids[rank]);
            if (!update.exec())
                return fail(update.lastError().text());
        }
    }

    return recordMigration(db, 1);
}

// user_version 1 → 2: folders→playlists, playlist_tracks N:N, drop tracks.position/folder_id.
bool Migrator::migrateTo2(QSqlDatabase &db)
{
    // Already on N:N?
    if (tableExists(db, QStringLiteral("playlist_tracks"))
        && tableExists(db, QStringLiteral("playlists"))
        && !columnExists(db, QStringLiteral("tracks"), QStringLiteral("folder_id"))) {
        return recordMigration(db, 2);
    }

    // --- playlists table (rename folders or create) ---
    if (tableExists(db, QStringLiteral("folders"))
        && !tableExists(db, QStringLiteral("playlists"))) {
        if (!execSql(db, QStringLiteral("ALTER TABLE folders RENAME TO playlists")))
            return false;
    }

    if (!tableExists(db, QStringLiteral("playlists"))) {
        if (!execSql(db, QStringLiteral(
                "CREATE TABLE playlists ("
                "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  name         TEXT    NOT NULL UNIQUE,"
                "  cover_color1 TEXT    NOT NULL DEFAULT '#e8a44a',"
                "  cover_color2 TEXT    NOT NULL DEFAULT '#d45d5d',"
                "  cover_image  TEXT    NOT NULL DEFAULT '',"
                "  dir_name     TEXT    NOT NULL DEFAULT '',"
                "  sort_mode    TEXT    NOT NULL DEFAULT 'custom',"
                "  created_at   INTEGER NOT NULL DEFAULT (strftime('%s','now')*1000)"
                ")"))) {
            return false;
        }
    }

    if (!columnExists(db, QStringLiteral("playlists"), QStringLiteral("dir_name"))) {
        if (!execSql(db, QStringLiteral(
                "ALTER TABLE playlists ADD COLUMN dir_name TEXT NOT NULL DEFAULT ''")))
            return false;
    }
    if (!columnExists(db, QStringLiteral("playlists"), QStringLiteral("sort_mode"))) {
        if (!execSql(db, QStringLiteral(
                "ALTER TABLE playlists ADD COLUMN sort_mode TEXT NOT NULL DEFAULT 'custom'")))
            return false;
    }

    // Seed dir_name from sanitize(name) where empty (matches MediaTools::sanitizeFileName).
    {
        QSqlQuery sel(db);
        if (!sel.exec(QStringLiteral("SELECT id, name, dir_name FROM playlists")))
            return fail(sel.lastError().text());
        QSqlQuery upd(db);
        upd.prepare(QStringLiteral("UPDATE playlists SET dir_name = ? WHERE id = ?"));
        while (sel.next()) {
            const int id = sel.value(0).toInt();
            const QString name = sel.value(1).toString();
            QString dir = sel.value(2).toString();
            if (dir.isEmpty() && id != 0) {
                dir = sanitizeDirName(name);
                upd.bindValue(0, dir);
                upd.bindValue(1, id);
                if (!upd.exec())
                    return fail(upd.lastError().text());
            }
        }
    }

    // --- playlist_tracks ---
    if (!execSql(db, QStringLiteral(
            "CREATE TABLE IF NOT EXISTS playlist_tracks ("
            "  playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,"
            "  track_id    INTEGER NOT NULL REFERENCES tracks(id)    ON DELETE CASCADE,"
            "  position    INTEGER NOT NULL,"
            "  added_at    INTEGER NOT NULL DEFAULT (strftime('%s','now')*1000),"
            "  PRIMARY KEY (playlist_id, track_id)"
            ") WITHOUT ROWID"))) {
        return false;
    }
    if (!execSql(db, QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_pt_order ON playlist_tracks(playlist_id, position)")))
        return false;
    if (!execSql(db, QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_pt_track ON playlist_tracks(track_id)")))
        return false;

    // Populate membership from tracks.folder_id when that column still exists.
    if (columnExists(db, QStringLiteral("tracks"), QStringLiteral("folder_id"))) {
        // Clear any partial run.
        if (!execSql(db, QStringLiteral("DELETE FROM playlist_tracks")))
            return false;

        QSqlQuery folders(db);
        if (!folders.exec(QStringLiteral(
                "SELECT DISTINCT folder_id FROM tracks "
                "WHERE folder_id IS NOT NULL AND folder_id != 0 "
                "ORDER BY folder_id"))) {
            return fail(folders.lastError().text());
        }

        QList<int> folderIds;
        while (folders.next())
            folderIds.append(folders.value(0).toInt());

        QSqlQuery select(db);
        select.prepare(QStringLiteral(
            "SELECT id, added_at FROM tracks WHERE folder_id = ? "
            "ORDER BY position, id"));

        QSqlQuery insert(db);
        insert.prepare(QStringLiteral(
            "INSERT INTO playlist_tracks (playlist_id, track_id, position, added_at) "
            "VALUES (?, ?, ?, ?)"));

        for (int folderId : folderIds) {
            // Skip if playlist row is gone (orphan folder_id).
            QSqlQuery exists(db);
            exists.prepare(QStringLiteral("SELECT 1 FROM playlists WHERE id = ?"));
            exists.addBindValue(folderId);
            if (!exists.exec() || !exists.next())
                continue;

            select.bindValue(0, folderId);
            if (!select.exec())
                return fail(select.lastError().text());

            int rank = 0;
            while (select.next()) {
                const int trackId = select.value(0).toInt();
                const qint64 addedAt = select.value(1).toLongLong();
                const qint64 pos = static_cast<qint64>(++rank) * 1024;
                insert.bindValue(0, folderId);
                insert.bindValue(1, trackId);
                insert.bindValue(2, pos);
                insert.bindValue(3, addedAt);
                if (!insert.exec())
                    return fail(insert.lastError().text());
            }
        }
    }

    // --- rebuild tracks without folder_id/position; with owner/liked_at/missing ---
    if (columnExists(db, QStringLiteral("tracks"), QStringLiteral("folder_id"))
        || !columnExists(db, QStringLiteral("tracks"), QStringLiteral("owner_playlist_id"))) {

        // Resolve duplicate file_path before UNIQUE index: keep lowest id.
        if (!execSql(db, QStringLiteral(
                "DELETE FROM tracks WHERE id NOT IN ("
                "  SELECT MIN(id) FROM tracks GROUP BY file_path"
                ")"))) {
            return false;
        }

        if (!execSql(db, QStringLiteral(
                "CREATE TABLE tracks_v2 ("
                "  id                INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  title             TEXT    NOT NULL,"
                "  artist            TEXT    NOT NULL DEFAULT 'Desconhecido',"
                "  file_path         TEXT    NOT NULL,"
                "  owner_playlist_id INTEGER REFERENCES playlists(id) ON DELETE SET NULL,"
                "  duration_ms       INTEGER NOT NULL DEFAULT 0,"
                "  cover_color1      TEXT    NOT NULL DEFAULT '#e8a44a',"
                "  cover_color2      TEXT    NOT NULL DEFAULT '#d45d5d',"
                "  liked             INTEGER NOT NULL DEFAULT 0 CHECK (liked IN (0,1)),"
                "  liked_at          INTEGER NOT NULL DEFAULT 0,"
                "  added_at          INTEGER NOT NULL DEFAULT (strftime('%s','now')*1000),"
                "  play_count        INTEGER NOT NULL DEFAULT 0,"
                "  last_played_at    INTEGER NOT NULL DEFAULT 0,"
                "  missing           INTEGER NOT NULL DEFAULT 0"
                ")"))) {
            return false;
        }

        // Map: folder_id 0 → NULL owner.
        // COALESCE: legacy rows often have NULL cover/artist; DEFAULT only applies
        // when a column is omitted from INSERT, not when SELECT yields NULL.
        if (!execSql(db, QStringLiteral(
                "INSERT INTO tracks_v2 ("
                "  id, title, artist, file_path, owner_playlist_id, duration_ms,"
                "  cover_color1, cover_color2, liked, liked_at, added_at,"
                "  play_count, last_played_at, missing"
                ") "
                "SELECT "
                "  id,"
                "  COALESCE(title, ''),"
                "  COALESCE(NULLIF(artist, ''), 'Desconhecido'),"
                "  file_path,"
                "  CASE WHEN folder_id IS NULL OR folder_id = 0 THEN NULL ELSE folder_id END,"
                "  COALESCE(duration_ms, 0),"
                "  COALESCE(NULLIF(cover_color1, ''), '#e8a44a'),"
                "  COALESCE(NULLIF(cover_color2, ''), '#d45d5d'),"
                "  COALESCE(liked, 0),"
                "  CASE WHEN COALESCE(liked, 0) = 1 THEN COALESCE(added_at, 0) ELSE 0 END,"
                "  COALESCE(added_at, 0),"
                "  COALESCE(play_count, 0),"
                "  COALESCE(last_played_at, 0),"
                "  0 "
                "FROM tracks"))) {
            return false;
        }

        // Orphan owners that don't exist in playlists → NULL.
        if (!execSql(db, QStringLiteral(
                "UPDATE tracks_v2 SET owner_playlist_id = NULL "
                "WHERE owner_playlist_id IS NOT NULL "
                "AND owner_playlist_id NOT IN (SELECT id FROM playlists)"))) {
            return false;
        }

        if (!execSql(db, QStringLiteral("DROP TABLE tracks")))
            return false;
        if (!execSql(db, QStringLiteral("ALTER TABLE tracks_v2 RENAME TO tracks")))
            return false;

        if (!execSql(db, QStringLiteral(
                "CREATE UNIQUE INDEX IF NOT EXISTS idx_tracks_path ON tracks(file_path)")))
            return false;
        if (!execSql(db, QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_tracks_liked ON tracks(liked) WHERE liked = 1")))
            return false;
        if (!execSql(db, QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_tracks_added ON tracks(added_at DESC)")))
            return false;
        if (!execSql(db, QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_tracks_played ON tracks(play_count DESC) "
                "WHERE play_count > 0")))
            return false;
    }

    // Retire sentinel playlist id 0.
    if (!execSql(db, QStringLiteral("DELETE FROM playlists WHERE id = 0")))
        return false;

    return recordMigration(db, 2);
}

// user_version 2 → 3: bookkeeping for LAN sync with the mobile app.
//
// Additive only: no existing table is touched. `tracks.id` and `playlists.id`
// are AUTOINCREMENT, so the phone can key off them directly — no per-row UUID
// is needed. `sync_meta` holds the server identity and the clientKey→playlist
// map that makes the phone's push idempotent.
bool Migrator::migrateTo3(QSqlDatabase &db)
{
    if (!execSql(db, QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sync_meta ("
            "  key   TEXT PRIMARY KEY,"
            "  value TEXT NOT NULL"
            ")")))
        return false;

    if (!execSql(db, QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sync_devices ("
            "  device_id    TEXT PRIMARY KEY,"
            "  name         TEXT NOT NULL DEFAULT '',"
            // Only the hash is stored: a leaked database must not hand out
            // working tokens.
            "  token_hash   TEXT NOT NULL,"
            "  created_at   INTEGER NOT NULL DEFAULT 0,"
            "  last_sync_at INTEGER NOT NULL DEFAULT 0"
            ")")))
        return false;

    // Marks playlists adopted from a phone, so the desktop knows the phone owns
    // their membership (ownership-by-origin rule).
    if (!columnExists(db, QStringLiteral("playlists"), QStringLiteral("origin_device"))) {
        if (!execSql(db, QStringLiteral(
                "ALTER TABLE playlists ADD COLUMN origin_device TEXT NOT NULL DEFAULT ''")))
            return false;
    }

    return recordMigration(db, 3);
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

    // Apply each pending migration in its own transaction.
    auto applyOne = [&](int from, int to, bool (*fn)(QSqlDatabase &)) -> bool {
        if (v >= to)
            return true;

        if (!execSql(db, QStringLiteral("PRAGMA foreign_keys = OFF")))
            return false;
        if (!db.transaction())
            return fail(QStringLiteral("BEGIN failed: %1").arg(db.lastError().text()));

        if (!fn(db)) {
            db.rollback();
            execSql(db, QStringLiteral("PRAGMA foreign_keys = ON"));
            return fail(QStringLiteral("migration to v%1 failed: %2 (backup: %3)")
                            .arg(to)
                            .arg(g_lastError, backupPath));
        }
        if (!execSql(db, QStringLiteral("PRAGMA user_version = %1").arg(to))) {
            db.rollback();
            execSql(db, QStringLiteral("PRAGMA foreign_keys = ON"));
            return false;
        }

        QSqlQuery fk(db);
        if (!fk.exec(QStringLiteral("PRAGMA foreign_key_check")) || fk.next()) {
            const QString detail = fk.isActive() && !fk.value(0).isNull()
                ? fk.value(0).toString()
                : fk.lastError().text();
            db.rollback();
            execSql(db, QStringLiteral("PRAGMA foreign_keys = ON"));
            return fail(QStringLiteral("foreign_key_check failed at v%1: %2 (backup: %3)")
                            .arg(to)
                            .arg(detail, backupPath));
        }

        if (!db.commit()) {
            execSql(db, QStringLiteral("PRAGMA foreign_keys = ON"));
            return fail(QStringLiteral("COMMIT failed at v%1: %2 (backup: %3)")
                            .arg(to)
                            .arg(db.lastError().text(), backupPath));
        }
        if (!execSql(db, QStringLiteral("PRAGMA foreign_keys = ON")))
            return false;

        v = to;
        Q_UNUSED(from);
        qInfo() << "Migrator: reached user_version" << v;
        return true;
    };

    if (!applyOne(0, 1, &Migrator::migrateTo1))
        return false;
    if (!applyOne(1, 2, &Migrator::migrateTo2))
        return false;
    if (!applyOne(2, 3, &Migrator::migrateTo3))
        return false;

    qInfo() << "Migrator: upgraded schema to user_version"
            << kSchemaVersion << "backup at" << backupPath;
    return true;
}

} // namespace lumen
