#include "database.h"
#include "migrator.h"
#include "mediatools.h"
#include "position_gap.h"
#include "owner_decision.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <QVariant>

Database &Database::instance()
{
    static Database db;
    return db;
}

bool Database::tableExists(const QString &name) const
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?"));
    q.addBindValue(name);
    return q.exec() && q.next();
}

bool Database::open()
{
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)
        && QSqlDatabase::database().isOpen()) {
        return true;
    }

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    const QString dbPath = dir + QStringLiteral("/vinil.db");

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        m_lastError = db.lastError().text();
        qWarning() << "Database::open failed:" << m_lastError;
        return false;
    }

    applySchema();

    if (!lumen::Migrator::run(db, dbPath)) {
        m_lastError = lumen::Migrator::lastError();
        qCritical() << "Database migration failed:" << m_lastError
                    << "backup:" << lumen::Migrator::lastBackupPath();
        return false;
    }

    // After migration, ensure v2 objects exist (fresh installs that jumped).
    ensureV2Schema();
    return true;
}

void Database::applySchema()
{
    QSqlQuery q;
    q.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    q.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    // Pre-migration shape so migrateTo1/2 have something to work on.
    // Post-migration open uses ensureV2Schema() (called after Migrator::run).
    if (!tableExists(QStringLiteral("playlists"))
        && !tableExists(QStringLiteral("folders"))) {
        ensureLegacySchema();
    } else if (tableExists(QStringLiteral("folders"))
               && !tableExists(QStringLiteral("playlists"))) {
        ensureLegacySchema();
    } else {
        ensureV2Schema();
    }

    ensurePlaybackStateSchema();
}

void Database::ensurePlaybackStateSchema()
{
    // RepeatMode: 0=Off, 1=All, 2=One. The original table only allowed 0/1, so
    // persisting "repeat one" failed the CHECK and was silently lost.
    static const char *kCreate = R"(
        CREATE TABLE playback_state (
            id               INTEGER PRIMARY KEY CHECK (id = 1),
            current_track_id INTEGER NOT NULL DEFAULT 0,
            position_ms      INTEGER NOT NULL DEFAULT 0,
            volume           REAL    NOT NULL DEFAULT 0.7
                                 CHECK (volume >= 0.0 AND volume <= 1.0),
            shuffle          INTEGER NOT NULL DEFAULT 0 CHECK (shuffle IN (0,1)),
            repeat_mode      INTEGER NOT NULL DEFAULT 0 CHECK (repeat_mode IN (0,1,2)),
            muted            INTEGER NOT NULL DEFAULT 0 CHECK (muted IN (0,1)),
            context_ids      TEXT    NOT NULL DEFAULT '',
            user_queue_ids   TEXT    NOT NULL DEFAULT '',
            context_index    INTEGER NOT NULL DEFAULT -1,
            context_name     TEXT    NOT NULL DEFAULT ''
        )
    )";

    auto addCol = [](const char *sql) {
        QSqlQuery a;
        a.exec(QString::fromUtf8(sql));
    };
    auto sqlText = [](const QString &s) {
        return s.isNull() ? QStringLiteral("") : s;
    };

    QSqlQuery q;
    QString ddl;
    if (tableExists(QStringLiteral("playback_state"))) {
        q.exec(QStringLiteral(
            "SELECT sql FROM sqlite_master WHERE type='table' AND name='playback_state'"));
        if (q.next())
            ddl = q.value(0).toString();
        q.finish();
    }

    if (ddl.isEmpty()) {
        q.exec(QString::fromUtf8(kCreate));
        q.exec(QStringLiteral("INSERT OR IGNORE INTO playback_state (id) VALUES (1)"));
        return;
    }

    // Always add missing columns first. v2.0.0 DBs often have the old
    // CHECK (repeat_mode IN (0,1)) plus a few ALTERs, but not context_index /
    // context_name — loadState's SELECT then fails and restore looks empty
    // even though volume/track/queue were written.
    addCol("ALTER TABLE playback_state ADD COLUMN muted INTEGER NOT NULL DEFAULT 0");
    addCol("ALTER TABLE playback_state ADD COLUMN context_ids TEXT NOT NULL DEFAULT ''");
    addCol("ALTER TABLE playback_state ADD COLUMN user_queue_ids TEXT NOT NULL DEFAULT ''");
    addCol("ALTER TABLE playback_state ADD COLUMN context_index INTEGER NOT NULL DEFAULT -1");
    addCol("ALTER TABLE playback_state ADD COLUMN context_name TEXT NOT NULL DEFAULT ''");
    q.exec(QStringLiteral("INSERT OR IGNORE INTO playback_state (id) VALUES (1)"));

    // IN (0,1,2) contains the substring IN (0,1) — require the missing ",2".
    const bool oldRepeatCheck =
        ddl.contains(QLatin1String("CHECK (repeat_mode IN (0,1))"))
        && !ddl.contains(QLatin1String("CHECK (repeat_mode IN (0,1,2))"));
    if (!oldRepeatCheck)
        return;

    int trackId = 0, shuffle = 0, repeatMode = 0, muted = 0, contextIndex = -1;
    qint64 posMs = 0;
    double volume = 0.7;
    QString contextIds, userQueueIds, contextName;

    QSqlQuery sel(QStringLiteral("SELECT * FROM playback_state WHERE id = 1"));
    if (sel.next()) {
        const QSqlRecord rec = sel.record();
        auto col = [&](const char *name) { return rec.indexOf(QLatin1String(name)); };
        if (col("current_track_id") >= 0)
            trackId = sel.value(col("current_track_id")).toInt();
        if (col("position_ms") >= 0)
            posMs = sel.value(col("position_ms")).toLongLong();
        if (col("volume") >= 0)
            volume = sel.value(col("volume")).toDouble();
        if (col("shuffle") >= 0)
            shuffle = sel.value(col("shuffle")).toInt();
        if (col("repeat_mode") >= 0)
            repeatMode = qBound(0, sel.value(col("repeat_mode")).toInt(), 2);
        if (col("muted") >= 0)
            muted = sel.value(col("muted")).toInt();
        if (col("context_ids") >= 0)
            contextIds = sel.value(col("context_ids")).toString();
        if (col("user_queue_ids") >= 0)
            userQueueIds = sel.value(col("user_queue_ids")).toString();
        if (col("context_index") >= 0)
            contextIndex = sel.value(col("context_index")).toInt();
        if (col("context_name") >= 0)
            contextName = sel.value(col("context_name")).toString();
    }
    sel.finish();

    if (!q.exec(QStringLiteral("DROP TABLE playback_state"))) {
        qWarning() << "ensurePlaybackStateSchema drop failed:" << q.lastError().text();
        return;
    }
    if (!q.exec(QString::fromUtf8(kCreate))) {
        qWarning() << "ensurePlaybackStateSchema recreate failed:" << q.lastError();
        return;
    }

    QSqlQuery ins;
    ins.prepare(QStringLiteral(
        "INSERT INTO playback_state ("
        "  id, current_track_id, position_ms, volume, shuffle, repeat_mode,"
        "  muted, context_ids, user_queue_ids, context_index, context_name"
        ") VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    ins.addBindValue(trackId);
    ins.addBindValue(posMs);
    ins.addBindValue(volume);
    ins.addBindValue(shuffle ? 1 : 0);
    ins.addBindValue(repeatMode);
    ins.addBindValue(muted ? 1 : 0);
    ins.addBindValue(sqlText(contextIds));
    ins.addBindValue(sqlText(userQueueIds));
    ins.addBindValue(contextIndex);
    ins.addBindValue(sqlText(contextName));
    if (!ins.exec())
        qWarning() << "ensurePlaybackStateSchema reinsert failed:" << ins.lastError();
}

void Database::ensureLegacySchema()
{
    QSqlQuery q;
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS folders (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            name         TEXT    NOT NULL UNIQUE,
            cover_color1 TEXT    NOT NULL DEFAULT '#e8a44a',
            cover_color2 TEXT    NOT NULL DEFAULT '#d45d5d',
            cover_image  TEXT    NOT NULL DEFAULT '',
            created_at   INTEGER NOT NULL DEFAULT (strftime('%s','now')*1000)
        )
    )");
    q.exec(QStringLiteral(
        "ALTER TABLE folders ADD COLUMN cover_image TEXT NOT NULL DEFAULT ''"));

    q.exec(R"(
        CREATE TABLE IF NOT EXISTS tracks (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            title        TEXT    NOT NULL,
            artist       TEXT    NOT NULL DEFAULT 'Desconhecido',
            folder_id    INTEGER NOT NULL DEFAULT 0,
            file_path    TEXT    NOT NULL,
            duration_ms  INTEGER NOT NULL DEFAULT 0,
            cover_color1 TEXT    NOT NULL DEFAULT '#e8a44a',
            cover_color2 TEXT    NOT NULL DEFAULT '#d45d5d',
            liked        INTEGER NOT NULL DEFAULT 0 CHECK (liked IN (0,1)),
            added_at     INTEGER NOT NULL DEFAULT (strftime('%s','now')*1000),
            play_count   INTEGER NOT NULL DEFAULT 0,
            last_played_at INTEGER NOT NULL DEFAULT 0,
            position     INTEGER NOT NULL DEFAULT 0,
            FOREIGN KEY (folder_id) REFERENCES folders(id) ON DELETE SET DEFAULT
        )
    )");
    q.exec(QStringLiteral(
        "ALTER TABLE tracks ADD COLUMN last_played_at INTEGER NOT NULL DEFAULT 0"));
    q.exec(QStringLiteral(
        "ALTER TABLE tracks ADD COLUMN position INTEGER NOT NULL DEFAULT 0"));
    q.exec(QStringLiteral(
        "INSERT OR IGNORE INTO folders (id, name, cover_color1, cover_color2) "
        "VALUES (0, '', '#e8a44a', '#d45d5d')"));
}

void Database::ensureV2Schema()
{
    QSqlQuery q;
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS playlists (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            name         TEXT    NOT NULL UNIQUE,
            cover_color1 TEXT    NOT NULL DEFAULT '#e8a44a',
            cover_color2 TEXT    NOT NULL DEFAULT '#d45d5d',
            cover_image  TEXT    NOT NULL DEFAULT '',
            dir_name     TEXT    NOT NULL DEFAULT '',
            sort_mode    TEXT    NOT NULL DEFAULT 'custom',
            created_at   INTEGER NOT NULL DEFAULT (strftime('%s','now')*1000)
        )
    )");
    q.exec(QStringLiteral(
        "ALTER TABLE playlists ADD COLUMN dir_name TEXT NOT NULL DEFAULT ''"));
    q.exec(QStringLiteral(
        "ALTER TABLE playlists ADD COLUMN sort_mode TEXT NOT NULL DEFAULT 'custom'"));

    q.exec(R"(
        CREATE TABLE IF NOT EXISTS tracks (
            id                INTEGER PRIMARY KEY AUTOINCREMENT,
            title             TEXT    NOT NULL,
            artist            TEXT    NOT NULL DEFAULT 'Desconhecido',
            file_path         TEXT    NOT NULL,
            owner_playlist_id INTEGER REFERENCES playlists(id) ON DELETE SET NULL,
            duration_ms       INTEGER NOT NULL DEFAULT 0,
            cover_color1      TEXT    NOT NULL DEFAULT '#e8a44a',
            cover_color2      TEXT    NOT NULL DEFAULT '#d45d5d',
            liked             INTEGER NOT NULL DEFAULT 0 CHECK (liked IN (0,1)),
            liked_at          INTEGER NOT NULL DEFAULT 0,
            added_at          INTEGER NOT NULL DEFAULT (strftime('%s','now')*1000),
            play_count        INTEGER NOT NULL DEFAULT 0,
            last_played_at    INTEGER NOT NULL DEFAULT 0,
            missing           INTEGER NOT NULL DEFAULT 0
        )
    )");
    q.exec(QStringLiteral(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_tracks_path ON tracks(file_path)"));
    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_tracks_liked ON tracks(liked) WHERE liked = 1"));
    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_tracks_added ON tracks(added_at DESC)"));
    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_tracks_played ON tracks(play_count DESC) "
        "WHERE play_count > 0"));

    q.exec(R"(
        CREATE TABLE IF NOT EXISTS playlist_tracks (
            playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,
            track_id    INTEGER NOT NULL REFERENCES tracks(id)    ON DELETE CASCADE,
            position    INTEGER NOT NULL,
            added_at    INTEGER NOT NULL DEFAULT (strftime('%s','now')*1000),
            PRIMARY KEY (playlist_id, track_id)
        ) WITHOUT ROWID
    )");
    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_pt_order ON playlist_tracks(playlist_id, position)"));
    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_pt_track ON playlist_tracks(track_id)"));
}

// --- Playlists ----------------------------------------------------------------

int Database::findOrCreatePlaylist(const QString &name, const QColor &c1, const QColor &c2)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT id FROM playlists WHERE name = ?"));
    q.addBindValue(name);
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return createPlaylist(name, c1, c2);
}

int Database::createPlaylist(const QString &name, const QColor &c1, const QColor &c2,
                             const QString &coverImage)
{
    const QString dirName = MediaTools::sanitizeFileName(name);
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO playlists "
        "(name, cover_color1, cover_color2, cover_image, dir_name) "
        "VALUES (?, ?, ?, ?, ?)"));
    q.addBindValue(name);
    q.addBindValue(c1.name());
    q.addBindValue(c2.name());
    q.addBindValue(coverImage.isEmpty() ? QStringLiteral("") : coverImage);
    q.addBindValue(dirName);
    q.exec();
    if (q.lastInsertId().toInt() > 0)
        return q.lastInsertId().toInt();

    q.prepare(QStringLiteral("SELECT id FROM playlists WHERE name = ?"));
    q.addBindValue(name);
    q.exec();
    return q.next() ? q.value(0).toInt() : 0;
}

QList<Folder> Database::allPlaylists()
{
    QList<Folder> result;
    QSqlQuery q(QStringLiteral(
        "SELECT id, name, cover_color1, cover_color2, cover_image, dir_name, sort_mode "
        "FROM playlists ORDER BY name"));
    while (q.next()) {
        Folder f;
        f.id         = q.value(0).toInt();
        f.name       = q.value(1).toString();
        f.cover.c1   = QColor(q.value(2).toString());
        f.cover.c2   = QColor(q.value(3).toString());
        f.coverImage = q.value(4).toString();
        f.dirName    = q.value(5).toString();
        f.sortMode   = q.value(6).toString();
        result.append(f);
    }
    return result;
}

Folder Database::playlistById(int id)
{
    Folder f;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT id, name, cover_color1, cover_color2, cover_image, dir_name, sort_mode "
        "FROM playlists WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        f.id         = q.value(0).toInt();
        f.name       = q.value(1).toString();
        f.cover.c1   = QColor(q.value(2).toString());
        f.cover.c2   = QColor(q.value(3).toString());
        f.coverImage = q.value(4).toString();
        f.dirName    = q.value(5).toString();
        f.sortMode   = q.value(6).toString();
    }
    return f;
}

Folder Database::playlistByName(const QString &name)
{
    Folder f;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT id, name, cover_color1, cover_color2, cover_image, dir_name, sort_mode "
        "FROM playlists WHERE name = ?"));
    q.addBindValue(name);
    if (q.exec() && q.next()) {
        f.id         = q.value(0).toInt();
        f.name       = q.value(1).toString();
        f.cover.c1   = QColor(q.value(2).toString());
        f.cover.c2   = QColor(q.value(3).toString());
        f.coverImage = q.value(4).toString();
        f.dirName    = q.value(5).toString();
        f.sortMode   = q.value(6).toString();
    }
    return f;
}

void Database::renamePlaylist(int id, const QString &newName)
{
    // dir_name is intentionally left unchanged so on-disk files stay findable.
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE playlists SET name = ? WHERE id = ?"));
    q.addBindValue(newName);
    q.addBindValue(id);
    q.exec();
}

void Database::updatePlaylistCover(int id, const QColor &c1, const QColor &c2)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE playlists SET cover_color1 = ?, cover_color2 = ? WHERE id = ?"));
    q.addBindValue(c1.name());
    q.addBindValue(c2.name());
    q.addBindValue(id);
    q.exec();
}

void Database::updatePlaylistCoverImage(int id, const QString &imagePath)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE playlists SET cover_image = ? WHERE id = ?"));
    q.addBindValue(imagePath.isEmpty() ? QStringLiteral("") : imagePath);
    q.addBindValue(id);
    q.exec();
}

void Database::deletePlaylist(int id)
{
    // playlist_tracks CASCADE; owner_playlist_id ON DELETE SET NULL.
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM playlists WHERE id = ?"));
    q.addBindValue(id);
    q.exec();
}

void Database::setPlaylistSortMode(int id, const QString &mode)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE playlists SET sort_mode = ? WHERE id = ?"));
    q.addBindValue(mode);
    q.addBindValue(id);
    q.exec();
}

QString Database::playlistSortMode(int id) const
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT sort_mode FROM playlists WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return QStringLiteral("custom");
}

QString Database::playlistDiskPath(int id) const
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT dir_name FROM playlists WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        const QString dir = q.value(0).toString();
        if (!dir.isEmpty()) {
            QString path = MediaTools::downloadDir() + QLatin1Char('/') + dir;
            QDir().mkpath(path);
            return path;
        }
    }
    return MediaTools::downloadDir();
}

QString Database::playlistDiskPathByName(const QString &name) const
{
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT id FROM playlists WHERE name = ?"));
    q.addBindValue(name);
    if (q.exec() && q.next())
        return playlistDiskPath(q.value(0).toInt());
    // Fallback for not-yet-created playlists: sanitize live name.
    return MediaTools::playlistDir(name);
}

QString Database::importCoverImage(const QString &sourcePath)
{
    if (sourcePath.isEmpty()) return QString();

    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/covers");
    QDir().mkpath(dir);

    QFileInfo fi(sourcePath);
    const QString dest = QStringLiteral("%1/cover_%2.%3")
        .arg(dir)
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(fi.suffix().isEmpty() ? QStringLiteral("png") : fi.suffix());

    if (QFile::copy(sourcePath, dest)) return dest;
    return sourcePath;
}

// --- Tracks -------------------------------------------------------------------

Track Database::rowToTrack(const QSqlQuery &q, int ownerNameCol, int positionCol) const
{
    Track t;
    t.id         = q.value(0).toInt();
    t.title      = q.value(1).toString();
    t.artist     = q.value(2).toString();
    t.audioUrl   = QUrl::fromLocalFile(q.value(3).toString());
    t.ownerPlaylistId = q.value(4).isNull() ? 0 : q.value(4).toInt();
    t.durationMs = q.value(5).toLongLong();
    t.cover.c1   = QColor(q.value(6).toString());
    t.cover.c2   = QColor(q.value(7).toString());
    t.liked      = q.value(8).toInt() == 1;
    t.likedAt    = q.value(9).toLongLong();
    t.addedAt    = q.value(10).toLongLong();
    t.playCount  = q.value(11).toInt();
    t.lastPlayedAt = q.value(12).toLongLong();
    t.missing    = q.value(13).toInt() == 1;

    // Display "primary" playlist = owner name when present.
    if (ownerNameCol >= 0) {
        t.folder = q.value(ownerNameCol).toString();
        t.folderId = t.ownerPlaylistId;
    } else {
        t.folderId = t.ownerPlaylistId;
    }
    if (positionCol >= 0)
        t.position = q.value(positionCol).toLongLong();
    return t;
}

int Database::insertTrack(const Track &t, int playlistId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO tracks "
        "(title, artist, file_path, owner_playlist_id, cover_color1, cover_color2, "
        " duration_ms) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(t.title);
    q.addBindValue(t.artist);
    q.addBindValue(t.audioUrl.toLocalFile());
    // Owner is the first playlist the track is added to; advisory only.
    if (playlistId > 0)
        q.addBindValue(playlistId);
    else
        q.addBindValue(QVariant()); // NULL
    q.addBindValue(t.cover.c1.name());
    q.addBindValue(t.cover.c2.name());
    q.addBindValue(t.durationMs);
    if (!q.exec()) {
        qWarning() << "insertTrack failed:" << q.lastError().text();
        return 0;
    }
    const int id = q.lastInsertId().toInt();
    if (playlistId > 0 && id > 0)
        addTrackToPlaylist(id, playlistId);
    return id;
}

void Database::updateTrack(int id, const QString &title, const QString &artist)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE tracks SET title = ?, artist = ? WHERE id = ?"));
    q.addBindValue(title);
    q.addBindValue(artist.isEmpty() ? QStringLiteral("Desconhecido") : artist);
    q.addBindValue(id);
    q.exec();
}

void Database::deleteTrack(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM tracks WHERE id = ?"));
    q.addBindValue(id);
    q.exec();
}

void Database::setLiked(int id, bool liked)
{
    QSqlQuery q;
    if (liked) {
        q.prepare(QStringLiteral(
            "UPDATE tracks SET liked = 1, liked_at = ? WHERE id = ?"));
        q.addBindValue(QDateTime::currentMSecsSinceEpoch());
        q.addBindValue(id);
    } else {
        q.prepare(QStringLiteral(
            "UPDATE tracks SET liked = 0, liked_at = 0 WHERE id = ?"));
        q.addBindValue(id);
    }
    q.exec();
}

void Database::setDuration(int id, qint64 ms)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE tracks SET duration_ms = ? WHERE id = ?"));
    q.addBindValue(ms);
    q.addBindValue(id);
    q.exec();
}

void Database::markPlayed(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE tracks SET play_count = play_count + 1, last_played_at = ? WHERE id = ?"));
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.addBindValue(id);
    q.exec();
}

void Database::incrementPlayCount(int id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE tracks SET play_count = play_count + 1 WHERE id = ?"));
    q.addBindValue(id);
    q.exec();
}

QList<Track> Database::allTracks()
{
    QList<Track> result;
    QSqlQuery q(QStringLiteral(
        "SELECT t.id, t.title, t.artist, t.file_path, t.owner_playlist_id,"
        "       t.duration_ms, t.cover_color1, t.cover_color2,"
        "       t.liked, t.liked_at, t.added_at, t.play_count,"
        "       t.last_played_at, t.missing,"
        "       COALESCE(p.name, '') AS owner_name "
        "FROM tracks t "
        "LEFT JOIN playlists p ON p.id = t.owner_playlist_id "
        "ORDER BY t.added_at DESC"));
    while (q.next())
        result.append(rowToTrack(q, 14, -1));
    return result;
}

QList<Track> Database::tracksInPlaylist(int playlistId)
{
    QList<Track> result;
    if (playlistId <= 0) return result;

    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT t.id, t.title, t.artist, t.file_path, t.owner_playlist_id,"
        "       t.duration_ms, t.cover_color1, t.cover_color2,"
        "       t.liked, t.liked_at, t.added_at, t.play_count,"
        "       t.last_played_at, t.missing,"
        "       COALESCE(p.name, '') AS owner_name,"
        "       pt.position "
        "FROM playlist_tracks pt "
        "JOIN tracks t ON t.id = pt.track_id "
        "LEFT JOIN playlists p ON p.id = t.owner_playlist_id "
        "WHERE pt.playlist_id = ? "
        "ORDER BY pt.position, t.id"));
    q.addBindValue(playlistId);
    if (!q.exec()) {
        qWarning() << "tracksInPlaylist failed:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        Track t = rowToTrack(q, 14, 15);
        t.folderId = playlistId; // viewing context
        result.append(t);
    }
    return result;
}

QList<Track> Database::tracksInPlaylistByName(const QString &name)
{
    const Folder f = playlistByName(name);
    if (f.id <= 0) return {};
    QList<Track> list = tracksInPlaylist(f.id);
    for (auto &t : list)
        t.folder = name;
    return list;
}

QList<int> Database::playlistIdsForTrack(int trackId)
{
    QList<int> ids;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT playlist_id FROM playlist_tracks WHERE track_id = ?"));
    q.addBindValue(trackId);
    if (q.exec()) {
        while (q.next())
            ids.append(q.value(0).toInt());
    }
    return ids;
}

// --- Membership ---------------------------------------------------------------

qint64 Database::nextPositionInPlaylist(int playlistId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT COALESCE(MAX(position), 0) + 1024 FROM playlist_tracks "
        "WHERE playlist_id = ?"));
    q.addBindValue(playlistId);
    if (q.exec() && q.next())
        return q.value(0).toLongLong();
    return 1024;
}

void Database::normalisePlaylistPositions(int playlistId)
{
    QSqlQuery sel;
    sel.prepare(QStringLiteral(
        "SELECT track_id FROM playlist_tracks WHERE playlist_id = ? "
        "ORDER BY position, track_id"));
    sel.addBindValue(playlistId);
    if (!sel.exec()) return;

    QList<int> ids;
    while (sel.next())
        ids.append(sel.value(0).toInt());

    setPlaylistTrackPositions(playlistId, lumen::position::renormalise(ids));
}

AddToPlaylistResult Database::addTrackToPlaylist(int trackId, int playlistId)
{
    AddToPlaylistResult r;
    if (trackId <= 0 || playlistId <= 0) {
        r.status = AddToPlaylistResult::Failed;
        return r;
    }

    // Already a member?
    {
        QSqlQuery q;
        q.prepare(QStringLiteral(
            "SELECT 1 FROM playlist_tracks WHERE playlist_id = ? AND track_id = ?"));
        q.addBindValue(playlistId);
        q.addBindValue(trackId);
        if (q.exec() && q.next()) {
            r.status = AddToPlaylistResult::AlreadyPresent;
            return r;
        }
    }

    // Load track + owner info.
    QString title;
    int ownerId = 0;
    {
        QSqlQuery q;
        q.prepare(QStringLiteral(
            "SELECT title, owner_playlist_id FROM tracks WHERE id = ?"));
        q.addBindValue(trackId);
        if (!q.exec() || !q.next()) {
            r.status = AddToPlaylistResult::Failed;
            return r;
        }
        title = q.value(0).toString();
        ownerId = q.value(1).isNull() ? 0 : q.value(1).toInt();
    }

    const Folder dest = playlistById(playlistId);
    r.trackTitle = title;
    r.destName = dest.name;

    // Decide what this association means (first owner / crosses owner / same owner) —
    // pure logic, no I/O — then act on the decision below.
    const lumen::playlist::OwnerDecision decision =
        lumen::playlist::decideOwner(ownerId, playlistId);

    // First association: set owner (file is not moved).
    if (decision.firstOwner) {
        QSqlQuery q;
        q.prepare(QStringLiteral(
            "UPDATE tracks SET owner_playlist_id = ? WHERE id = ? "
            "AND owner_playlist_id IS NULL"));
        q.addBindValue(playlistId);
        q.addBindValue(trackId);
        q.exec();
        r.firstOwner = true;
    }

    const qint64 pos = nextPositionInPlaylist(playlistId);
    QSqlQuery ins;
    ins.prepare(QStringLiteral(
        "INSERT INTO playlist_tracks (playlist_id, track_id, position, added_at) "
        "VALUES (?, ?, ?, ?)"));
    ins.addBindValue(playlistId);
    ins.addBindValue(trackId);
    ins.addBindValue(pos);
    ins.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (!ins.exec()) {
        qWarning() << "addTrackToPlaylist insert failed:" << ins.lastError().text();
        r.status = AddToPlaylistResult::Failed;
        return r;
    }

    r.status = AddToPlaylistResult::Added;
    if (decision.crossesOwner) {
        r.crossesOwner = true;
        const Folder owner = playlistById(decision.ownerPlaylistId);
        r.ownerName = owner.name;
        r.ownerDirPath = playlistDiskPath(decision.ownerPlaylistId);
    }
    return r;
}

bool Database::removeTrackFromPlaylist(int trackId, int playlistId)
{
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "DELETE FROM playlist_tracks WHERE playlist_id = ? AND track_id = ?"));
    q.addBindValue(playlistId);
    q.addBindValue(trackId);
    if (!q.exec())
        return false;
    // Owner is advisory and never cleared here — file stays where it is.
    return true;
}

void Database::setPlaylistTrackPositions(int playlistId,
                                         const QList<QPair<int, qint64>> &positions)
{
    if (positions.isEmpty()) return;

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        qWarning() << "setPlaylistTrackPositions: BEGIN failed";
        return;
    }

    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE playlist_tracks SET position = ? "
        "WHERE playlist_id = ? AND track_id = ?"));
    for (const auto &pair : positions) {
        q.bindValue(0, pair.second);
        q.bindValue(1, playlistId);
        q.bindValue(2, pair.first);
        if (!q.exec()) {
            qWarning() << "setPlaylistTrackPositions failed:" << q.lastError().text();
            db.rollback();
            return;
        }
    }

    if (!db.commit()) {
        qWarning() << "setPlaylistTrackPositions: COMMIT failed";
        db.rollback();
    }
}

int Database::seedFakeLibrary(int n)
{
    if (n <= 0) return 0;

    static const char *kNames[] = {
        "Seed A", "Seed B", "Seed C", "Seed D", "Seed E"
    };
    int playlistIds[5];
    for (int i = 0; i < 5; ++i) {
        playlistIds[i] = findOrCreatePlaylist(
            QString::fromLatin1(kNames[i]),
            QColor(QStringLiteral("#e8a44a")),
            QColor(QStringLiteral("#d45d5d")));
    }

    int inserted = 0;
    for (int i = 0; i < n; ++i) {
        Track t;
        t.title = QStringLiteral("Fake Track %1").arg(i + 1);
        t.artist = QStringLiteral("Fake Artist %1").arg((i % 50) + 1);
        t.audioUrl = QUrl::fromLocalFile(
            QStringLiteral("C:/lumen-seed/fake_%1.opus").arg(i + 1));
        t.durationMs = 180000 + (i % 120) * 1000;
        t.cover = Theme::randomPalette();
        if (insertTrack(t, playlistIds[i % 5]) > 0)
            ++inserted;
    }
    qInfo() << "seedFakeLibrary: inserted" << inserted << "tracks";
    return inserted;
}

// --- Playback state -----------------------------------------------------------

namespace {

QString joinIds(const QList<int> &ids)
{
    // QStringList::join on an empty list returns a *null* QString, which Qt
    // binds as SQL NULL and trips the NOT NULL columns — the whole UPDATE
    // then fails and volume/track/queue are all lost (#20, #21).
    if (ids.isEmpty())
        return QStringLiteral("");
    QStringList parts;
    parts.reserve(ids.size());
    for (int id : ids)
        parts.append(QString::number(id));
    return parts.join(QLatin1Char(','));
}

QList<int> splitIds(const QString &csv)
{
    QList<int> ids;
    if (csv.isEmpty()) return ids;
    const auto parts = csv.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        bool ok = false;
        const int id = p.toInt(&ok);
        if (ok && id > 0) ids.append(id);
    }
    return ids;
}

} // namespace

Database::PlaybackState Database::loadState()
{
    ensurePlaybackStateSchema();

    PlaybackState s;
    QSqlQuery q(QStringLiteral(
        "SELECT current_track_id, position_ms, volume, shuffle, repeat_mode, "
        "       muted, context_ids, user_queue_ids, context_index, context_name "
        "FROM playback_state WHERE id = 1"));
    if (!q.exec()) {
        qWarning() << "loadState failed:" << q.lastError().text();
        return s;
    }
    if (q.next()) {
        s.trackId    = q.value(0).toInt();
        s.posMs      = q.value(1).toLongLong();
        s.volume     = q.value(2).toDouble();
        s.shuffle    = q.value(3).toInt() == 1;
        s.repeatMode = qBound(0, q.value(4).toInt(), 2);
        s.muted      = q.value(5).toInt() == 1;
        s.contextIds = splitIds(q.value(6).toString());
        s.userQueueIds = splitIds(q.value(7).toString());
        s.contextIndex = q.value(8).toInt();
        s.contextName  = q.value(9).toString();
    }
    return s;
}

void Database::saveState(const PlaybackState &s)
{
    ensurePlaybackStateSchema();

    QSqlQuery q;
    q.prepare(QStringLiteral(
        "UPDATE playback_state "
        "SET current_track_id = ?, position_ms = ?, volume = ?, "
        "    shuffle = ?, repeat_mode = ?, muted = ?, "
        "    context_ids = ?, user_queue_ids = ?, "
        "    context_index = ?, context_name = ? "
        "WHERE id = 1"));
    q.addBindValue(s.trackId);
    q.addBindValue(s.posMs);
    q.addBindValue(s.volume);
    q.addBindValue(s.shuffle ? 1 : 0);
    q.addBindValue(qBound(0, s.repeatMode, 2));
    q.addBindValue(s.muted ? 1 : 0);
    q.addBindValue(joinIds(s.contextIds));
    q.addBindValue(joinIds(s.userQueueIds));
    q.addBindValue(s.contextIndex);
    // Null QString binds as SQL NULL; column is NOT NULL DEFAULT ''.
    q.addBindValue(s.contextName.isNull() ? QStringLiteral("") : s.contextName);
    if (!q.exec()) {
        qWarning() << "saveState failed:" << q.lastError().text();
        return;
    }
    if (q.numRowsAffected() == 0) {
        QSqlQuery ins;
        ins.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO playback_state ("
            "  id, current_track_id, position_ms, volume, shuffle, repeat_mode,"
            "  muted, context_ids, user_queue_ids, context_index, context_name"
            ") VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        ins.addBindValue(s.trackId);
        ins.addBindValue(s.posMs);
        ins.addBindValue(s.volume);
        ins.addBindValue(s.shuffle ? 1 : 0);
        ins.addBindValue(qBound(0, s.repeatMode, 2));
        ins.addBindValue(s.muted ? 1 : 0);
        ins.addBindValue(joinIds(s.contextIds));
        ins.addBindValue(joinIds(s.userQueueIds));
        ins.addBindValue(s.contextIndex);
        ins.addBindValue(s.contextName.isNull() ? QStringLiteral("") : s.contextName);
        if (!ins.exec())
            qWarning() << "saveState insert failed:" << ins.lastError().text();
    }
}
