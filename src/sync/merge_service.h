#ifndef LUMEN_SYNC_MERGE_SERVICE_H
#define LUMEN_SYNC_MERGE_SERVICE_H

#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>

namespace lumen::sync {

// Applies the phone's additive push. The desktop stays the source of truth for
// files and the library; only light state travels upwards.
//
// Merge rules (see PLANEJAMENTO §3.2 in the mobile repo):
//  - likes:      last-write-wins on liked_at, which covers like AND unlike
//                without needing tombstones.
//  - playCounts: additive by delta; last_played_at takes the max.
//  - playlists created on the phone: idempotent by clientKey, so a retried push
//    adopts the same playlist instead of duplicating it.
//  - membership: only for playlists the phone owns (ownership by origin).
//
// Runs in a single transaction: a partial merge would leave the library in a
// state neither side believes in.
struct MergeResult {
    bool        ok = false;
    QString     error;
    QJsonObject response;   // { createdPlaylists: [{clientKey, id}], skipped: [...] }
    bool        changed = false;
};

MergeResult applyPush(QSqlDatabase &db, const QString &deviceId, const QJsonObject &payload);

} // namespace lumen::sync

#endif // LUMEN_SYNC_MERGE_SERVICE_H
