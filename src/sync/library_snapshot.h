#ifndef LUMEN_SYNC_LIBRARY_SNAPSHOT_H
#define LUMEN_SYNC_LIBRARY_SNAPSHOT_H

#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>

namespace lumen::sync {

// Wire protocol version. Bump only on an incompatible change; the phone refuses
// to sync against a different value instead of silently corrupting its library.
inline constexpr int kProtocolVersion = 1;

// Full library snapshot as JSON.
//
// There is no delta protocol on purpose: a personal library is 10^3–10^4 tracks,
// roughly 1.5 MB of JSON, which is sub-second on a LAN. A delta would require
// change-tracking plus tombstones on the desktop — permanent new state to keep
// correct — to save about a megabyte per sync. Deletions come for free: whatever
// is missing from the snapshot no longer exists.
//
// Pure function over a database connection, so it can be unit-tested against an
// in-memory database.
QJsonObject buildLibrarySnapshot(QSqlDatabase &db, const QString &serverId);

// Reads (creating on first use) the stable server identity. If the user ever
// recreates vinil.db this changes, which is exactly what tells the phone to
// re-pair and resync from scratch.
QString serverId(QSqlDatabase &db);

QString readMeta(QSqlDatabase &db, const QString &key);
bool    writeMeta(QSqlDatabase &db, const QString &key, const QString &value);

} // namespace lumen::sync

#endif // LUMEN_SYNC_LIBRARY_SNAPSHOT_H
