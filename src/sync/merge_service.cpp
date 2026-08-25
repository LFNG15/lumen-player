#include "merge_service.h"

#include "library_snapshot.h"

#include <QDateTime>
#include <QJsonArray>
#include <QSqlError>
#include <QSqlQuery>

namespace lumen::sync {

namespace {

// clientKey → playlist id, so a retried push adopts instead of duplicating.
QString clientKeyMetaKey(const QString &clientKey)
{
    return QStringLiteral("client_playlist:") + clientKey;
}

bool trackExists(QSqlDatabase &db, qlonglong id)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT 1 FROM tracks WHERE id = ?"));
    q.addBindValue(id);
    return q.exec() && q.next();
}

// Appends " (celular)" until the name is free. The desktop enforces UNIQUE on
// playlists.name, so a collision would otherwise abort the whole merge.
QString uniquePlaylistName(QSqlDatabase &db, const QString &wanted)
{
    auto taken = [&db](const QString &name) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT 1 FROM playlists WHERE name = ?"));
        q.addBindValue(name);
        return q.exec() && q.next();
    };

    if (!taken(wanted))
        return wanted;

    QString candidate = wanted + QStringLiteral(" (celular)");
    int suffix = 2;
    while (taken(candidate)) {
        candidate = QStringLiteral("%1 (celular %2)").arg(wanted).arg(suffix++);
        if (suffix > 100)
            break;
    }
    return candidate;
}

} // namespace

MergeResult applyPush(QSqlDatabase &db, const QString &deviceId, const QJsonObject &payload)
{
    MergeResult result;
    QJsonArray skipped;
    QJsonArray createdPlaylists;

    if (!db.transaction()) {
        result.error = QStringLiteral("BEGIN failed: %1").arg(db.lastError().text());
        return result;
    }

    auto rollback = [&db, &result](const QString &message) {
        db.rollback();
        result.ok = false;
        result.error = message;
        return result;
    };

    // --- likes: last-write-wins on liked_at ---
    const QJsonArray likes = payload.value(QStringLiteral("likes")).toArray();
    for (const QJsonValue &value : likes) {
        const QJsonObject item = value.toObject();
        const qlonglong trackId = item.value(QStringLiteral("trackId")).toVariant().toLongLong();
        const bool liked = item.value(QStringLiteral("liked")).toBool();
        const qlonglong likedAt = item.value(QStringLiteral("likedAt")).toVariant().toLongLong();

        if (!trackExists(db, trackId)) {
            skipped.append(trackId);
            continue;
        }

        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "UPDATE tracks SET liked = ?, liked_at = ? "
            "WHERE id = ? AND ? > liked_at"));
        q.addBindValue(liked ? 1 : 0);
        q.addBindValue(likedAt);
        q.addBindValue(trackId);
        q.addBindValue(likedAt);
        if (!q.exec())
            return rollback(q.lastError().text());
        if (q.numRowsAffected() > 0)
            result.changed = true;
    }

    // --- play counts: additive by delta ---
    const QJsonArray plays = payload.value(QStringLiteral("playCounts")).toArray();
    for (const QJsonValue &value : plays) {
        const QJsonObject item = value.toObject();
        const qlonglong trackId = item.value(QStringLiteral("trackId")).toVariant().toLongLong();
        const int delta = item.value(QStringLiteral("delta")).toInt();
        const qlonglong lastPlayedAt =
            item.value(QStringLiteral("lastPlayedAt")).toVariant().toLongLong();

        if (delta <= 0)
            continue;
        if (!trackExists(db, trackId)) {
            skipped.append(trackId);
            continue;
        }

        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "UPDATE tracks SET play_count = play_count + ?, "
            "                  last_played_at = MAX(last_played_at, ?) "
            "WHERE id = ?"));
        q.addBindValue(delta);
        q.addBindValue(lastPlayedAt);
        q.addBindValue(trackId);
        if (!q.exec())
            return rollback(q.lastError().text());
        result.changed = true;
    }

    // --- playlists created on the phone (idempotent by clientKey) ---
    const QJsonArray newPlaylists = payload.value(QStringLiteral("newPlaylists")).toArray();
    for (const QJsonValue &value : newPlaylists) {
        const QJsonObject item = value.toObject();
        const QString clientKey = item.value(QStringLiteral("clientKey")).toString();
        if (clientKey.isEmpty())
            continue;

        const QString metaKey = clientKeyMetaKey(clientKey);
        const QString known = readMeta(db, metaKey);

        qlonglong playlistId = 0;
        if (!known.isEmpty()) {
            // Already adopted in an earlier push — reuse it.
            playlistId = known.toLongLong();
        } else {
            const QString wanted = item.value(QStringLiteral("name")).toString().trimmed();
            if (wanted.isEmpty())
                continue;

            QSqlQuery insert(db);
            insert.prepare(QStringLiteral(
                "INSERT INTO playlists (name, cover_color1, cover_color2, cover_image, "
                "                       dir_name, sort_mode, created_at, origin_device) "
                "VALUES (?, ?, ?, '', ?, 'custom', ?, ?)"));
            const QString name = uniquePlaylistName(db, wanted);
            insert.addBindValue(name);
            insert.addBindValue(item.value(QStringLiteral("coverColor1")).toString());
            insert.addBindValue(item.value(QStringLiteral("coverColor2")).toString());
            insert.addBindValue(name);
            insert.addBindValue(QDateTime::currentMSecsSinceEpoch());
            insert.addBindValue(deviceId);
            if (!insert.exec())
                return rollback(insert.lastError().text());

            playlistId = insert.lastInsertId().toLongLong();
            if (!writeMeta(db, metaKey, QString::number(playlistId)))
                return rollback(QStringLiteral("could not record clientKey mapping"));
            result.changed = true;
        }

        // Membership: replace wholesale — the phone owns this playlist.
        QSqlQuery clear(db);
        clear.prepare(QStringLiteral("DELETE FROM playlist_tracks WHERE playlist_id = ?"));
        clear.addBindValue(playlistId);
        if (!clear.exec())
            return rollback(clear.lastError().text());

        const QJsonArray trackIds = item.value(QStringLiteral("trackIds")).toArray();
        qlonglong position = 1024;
        for (const QJsonValue &idValue : trackIds) {
            const qlonglong trackId = idValue.toVariant().toLongLong();
            if (!trackExists(db, trackId)) {
                skipped.append(trackId);
                continue;
            }
            QSqlQuery link(db);
            link.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO playlist_tracks (playlist_id, track_id, position, added_at) "
                "VALUES (?, ?, ?, ?)"));
            link.addBindValue(playlistId);
            link.addBindValue(trackId);
            link.addBindValue(position);
            link.addBindValue(QDateTime::currentMSecsSinceEpoch());
            if (!link.exec())
                return rollback(link.lastError().text());
            position += 1024;
            result.changed = true;
        }

        QJsonObject created;
        created[QStringLiteral("clientKey")] = clientKey;
        created[QStringLiteral("id")] = playlistId;
        createdPlaylists.append(created);
    }

    // --- membership updates for playlists the phone already owns ---
    const QJsonArray membership = payload.value(QStringLiteral("playlistMembership")).toArray();
    for (const QJsonValue &value : membership) {
        const QJsonObject item = value.toObject();
        const qlonglong playlistId =
            item.value(QStringLiteral("playlistId")).toVariant().toLongLong();

        // Ownership by origin: a playlist born on the desktop is never rewritten
        // from the phone.
        QSqlQuery owner(db);
        owner.prepare(QStringLiteral("SELECT origin_device FROM playlists WHERE id = ?"));
        owner.addBindValue(playlistId);
        if (!owner.exec() || !owner.next())
            continue;
        if (owner.value(0).toString() != deviceId)
            continue;

        QSqlQuery clear(db);
        clear.prepare(QStringLiteral("DELETE FROM playlist_tracks WHERE playlist_id = ?"));
        clear.addBindValue(playlistId);
        if (!clear.exec())
            return rollback(clear.lastError().text());

        const QJsonArray trackIds = item.value(QStringLiteral("trackIds")).toArray();
        qlonglong position = 1024;
        for (const QJsonValue &idValue : trackIds) {
            const qlonglong trackId = idValue.toVariant().toLongLong();
            if (!trackExists(db, trackId)) {
                skipped.append(trackId);
                continue;
            }
            QSqlQuery link(db);
            link.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO playlist_tracks (playlist_id, track_id, position, added_at) "
                "VALUES (?, ?, ?, ?)"));
            link.addBindValue(playlistId);
            link.addBindValue(trackId);
            link.addBindValue(position);
            link.addBindValue(QDateTime::currentMSecsSinceEpoch());
            if (!link.exec())
                return rollback(link.lastError().text());
            position += 1024;
            result.changed = true;
        }
    }

    if (!db.commit())
        return rollback(QStringLiteral("COMMIT failed: %1").arg(db.lastError().text()));

    result.ok = true;
    result.response[QStringLiteral("createdPlaylists")] = createdPlaylists;
    result.response[QStringLiteral("skipped")] = skipped;
    return result;
}

} // namespace lumen::sync
