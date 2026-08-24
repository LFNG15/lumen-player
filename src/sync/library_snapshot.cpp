#include "library_snapshot.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace lumen::sync {

namespace {
constexpr const char *kServerIdKey = "server_id";
}

QString readMeta(QSqlDatabase &db, const QString &key)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT value FROM sync_meta WHERE key = ?"));
    q.addBindValue(key);
    if (!q.exec() || !q.next())
        return {};
    return q.value(0).toString();
}

bool writeMeta(QSqlDatabase &db, const QString &key, const QString &value)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO sync_meta (key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    q.addBindValue(key);
    q.addBindValue(value);
    return q.exec();
}

QString serverId(QSqlDatabase &db)
{
    QString id = readMeta(db, QLatin1String(kServerIdKey));
    if (!id.isEmpty())
        return id;

    id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    writeMeta(db, QLatin1String(kServerIdKey), id);
    return id;
}

QJsonObject buildLibrarySnapshot(QSqlDatabase &db, const QString &serverIdValue)
{
    QJsonObject root;
    root[QStringLiteral("serverId")] = serverIdValue;
    root[QStringLiteral("proto")] = kProtocolVersion;
    root[QStringLiteral("generatedAt")] = QDateTime::currentMSecsSinceEpoch();

    // --- playlists ---
    QJsonArray playlists;
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral(
                "SELECT id, name, cover_color1, cover_color2, cover_image, sort_mode, created_at "
                "FROM playlists ORDER BY id"))) {
            while (q.next()) {
                QJsonObject p;
                p[QStringLiteral("id")] = q.value(0).toLongLong();
                p[QStringLiteral("name")] = q.value(1).toString();
                p[QStringLiteral("coverColor1")] = q.value(2).toString();
                p[QStringLiteral("coverColor2")] = q.value(3).toString();
                // The image itself travels over /v1/playlists/{id}/cover, not inline.
                p[QStringLiteral("hasCoverImage")] = !q.value(4).toString().isEmpty();
                p[QStringLiteral("sortMode")] = q.value(5).toString();
                p[QStringLiteral("createdAt")] = q.value(6).toLongLong();
                playlists.append(p);
            }
        }
    }
    root[QStringLiteral("playlists")] = playlists;

    // --- tracks ---
    QJsonArray tracks;
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral(
                "SELECT id, title, artist, file_path, owner_playlist_id, duration_ms, "
                "       cover_color1, cover_color2, liked, liked_at, added_at, "
                "       play_count, last_played_at, missing "
                "FROM tracks ORDER BY id"))) {
            while (q.next()) {
                QJsonObject t;
                t[QStringLiteral("id")] = q.value(0).toLongLong();
                t[QStringLiteral("title")] = q.value(1).toString();
                t[QStringLiteral("artist")] = q.value(2).toString();
                t[QStringLiteral("durationMs")] = q.value(5).toLongLong();
                t[QStringLiteral("coverColor1")] = q.value(6).toString();
                t[QStringLiteral("coverColor2")] = q.value(7).toString();

                const QVariant owner = q.value(4);
                if (owner.isNull())
                    t[QStringLiteral("ownerPlaylistId")] = QJsonValue::Null;
                else
                    t[QStringLiteral("ownerPlaylistId")] = owner.toLongLong();

                t[QStringLiteral("liked")] = q.value(8).toInt() != 0;
                t[QStringLiteral("likedAt")] = q.value(9).toLongLong();
                t[QStringLiteral("addedAt")] = q.value(10).toLongLong();
                t[QStringLiteral("playCount")] = q.value(11).toInt();
                t[QStringLiteral("lastPlayedAt")] = q.value(12).toLongLong();

                // Content identity for the phone's re-download decision. The
                // absolute path is deliberately NOT exposed: it is useless to the
                // phone and would leak the disk layout.
                const QFileInfo info(q.value(3).toString());
                const bool exists = info.exists() && info.isFile();
                t[QStringLiteral("missing")] = q.value(13).toInt() != 0 || !exists;
                t[QStringLiteral("fileSize")] = exists ? info.size() : 0;
                t[QStringLiteral("fileMtime")] =
                    exists ? info.lastModified().toSecsSinceEpoch() : 0;

                tracks.append(t);
            }
        }
    }
    root[QStringLiteral("tracks")] = tracks;

    // --- membership ---
    QJsonArray links;
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral(
                "SELECT playlist_id, track_id, position, added_at "
                "FROM playlist_tracks ORDER BY playlist_id, position"))) {
            while (q.next()) {
                QJsonObject l;
                l[QStringLiteral("playlistId")] = q.value(0).toLongLong();
                l[QStringLiteral("trackId")] = q.value(1).toLongLong();
                l[QStringLiteral("position")] = q.value(2).toLongLong();
                l[QStringLiteral("addedAt")] = q.value(3).toLongLong();
                links.append(l);
            }
        }
    }
    root[QStringLiteral("playlistTracks")] = links;

    // --- playback state (single row) ---
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral(
                "SELECT current_track_id, position_ms, shuffle, repeat_mode, "
                "       context_ids, user_queue_ids, context_index, context_name "
                "FROM playback_state WHERE id = 1"))
            && q.next()) {
            QJsonObject s;
            s[QStringLiteral("currentTrackId")] = q.value(0).toLongLong();
            s[QStringLiteral("positionMs")] = q.value(1).toLongLong();
            s[QStringLiteral("shuffle")] = q.value(2).toInt() != 0;
            s[QStringLiteral("repeatMode")] = q.value(3).toInt();

            auto splitIds = [](const QString &csv) {
                QJsonArray out;
                const auto parts = csv.split(QLatin1Char(','), Qt::SkipEmptyParts);
                for (const auto &part : parts) {
                    bool ok = false;
                    const qlonglong id = part.trimmed().toLongLong(&ok);
                    if (ok) out.append(id);
                }
                return out;
            };
            s[QStringLiteral("contextIds")] = splitIds(q.value(4).toString());
            s[QStringLiteral("userQueueIds")] = splitIds(q.value(5).toString());
            s[QStringLiteral("contextIndex")] = q.value(6).toInt();
            s[QStringLiteral("contextName")] = q.value(7).toString();
            root[QStringLiteral("playbackState")] = s;
        }
    }

    return root;
}

} // namespace lumen::sync
