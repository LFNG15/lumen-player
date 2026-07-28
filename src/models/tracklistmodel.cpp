#include "tracklistmodel.h"
#include "textutils.h"
#include "database.h"

#include <QMimeData>
#include <QDataStream>
#include <QIODevice>
#include <algorithm>

static const char kMime[] = "application/x-lumen-trackids";

const char *TrackListModel::mimeType() { return kMime; }

TrackListModel::TrackListModel(TrackModel *library, QObject *parent)
    : QAbstractListModel(parent)
    , m_library(library)
{
    // Pages call reload()/setSource() from their own refresh() so they can
    // re-apply client-side sort modes after the data reload.
}

void TrackListModel::setSource(Source src)
{
    m_source = src;
    reload();
}

void TrackListModel::setPlaybackState(int currentTrackId, bool isPlaying)
{
    if (m_currentId == currentTrackId && m_playing == isPlaying)
        return;
    m_currentId = currentTrackId;
    m_playing = isPlaying;
    if (!m_ids.isEmpty())
        emit dataChanged(index(0, 0), index(m_ids.size() - 1, 0),
                         {IsCurrentRole, IsPlayingRole, IndexLabelRole});
}

void TrackListModel::setReorderEnabled(bool on)
{
    if (m_reorderEnabled == on) return;
    m_reorderEnabled = on;
    if (!m_ids.isEmpty())
        emit dataChanged(index(0, 0), index(m_ids.size() - 1, 0));
}

void TrackListModel::reload()
{
    if (!m_library) return;

    QVector<int> ids;
    switch (m_source.kind) {
    case Source::All:
        for (const auto &t : m_library->tracks())
            ids.append(t.id);
        break;
    case Source::Playlist: {
        QList<Track> list;
        if (m_source.playlistId > 0)
            list = Database::instance().tracksInPlaylist(m_source.playlistId);
        else if (!m_source.playlistName.isEmpty())
            list = m_library->tracksInFolder(m_source.playlistName);
        for (const auto &t : list)
            ids.append(t.id);
        break;
    }
    case Source::Liked:
        for (const auto &t : m_library->likedTracks())
            ids.append(t.id);
        break;
    case Source::Standalone:
        for (const auto &t : m_library->standaloneTracks())
            ids.append(t.id);
        break;
    }

    beginResetModel();
    m_ids = ids;
    endResetModel();
}

void TrackListModel::setOrderedIds(const QVector<int> &ids)
{
    beginResetModel();
    m_ids = ids;
    endResetModel();
}

int TrackListModel::trackIdAt(int row) const
{
    if (row < 0 || row >= m_ids.size()) return 0;
    return m_ids[row];
}

QList<Track> TrackListModel::tracksInOrder() const
{
    QList<Track> out;
    out.reserve(m_ids.size());
    for (int id : m_ids) {
        if (Track *t = m_library->findTrack(id))
            out.append(*t);
    }
    return out;
}

int TrackListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_ids.size();
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {ArtistRole, "artist"},
        {DurationRole, "duration"},
        {Color1Role, "color1"},
        {Color2Role, "color2"},
        {LikedRole, "liked"},
        {PlayCountRole, "playCount"},
        {AddedAtRole, "addedAt"},
        {LastPlayedRole, "lastPlayed"},
        {OwnerNameRole, "ownerName"},
        {MissingRole, "missing"},
        {IsCurrentRole, "isCurrent"},
        {IsPlayingRole, "isPlaying"},
        {SearchKeyRole, "searchKey"},
        {PositionRole, "position"},
        {IndexLabelRole, "indexLabel"},
        {Qt::AccessibleTextRole, "accessible"},
    };
}

QVariant TrackListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !m_library) return {};
    const int row = index.row();
    if (row < 0 || row >= m_ids.size()) return {};

    const Track *t = m_library->findTrack(m_ids[row]);
    if (!t) return {};

    switch (role) {
    case IdRole: return t->id;
    case TitleRole: return t->title;
    case ArtistRole: return t->artist;
    case DurationRole: return t->durationMs;
    case Color1Role: return t->cover.c1;
    case Color2Role: return t->cover.c2;
    case LikedRole: return t->liked;
    case PlayCountRole: return t->playCount;
    case AddedAtRole: return t->addedAt;
    case LastPlayedRole: return t->lastPlayedAt;
    case OwnerNameRole: return t->folder;
    case MissingRole: return t->missing;
    case IsCurrentRole: return t->id == m_currentId;
    case IsPlayingRole: return t->id == m_currentId && m_playing;
    case SearchKeyRole:
        return TextUtils::normalized(t->title + QLatin1Char(' ') + t->artist
                                     + QLatin1Char(' ') + t->folder);
    case PositionRole: return t->position;
    case IndexLabelRole:
        return QStringLiteral("%1").arg(row + 1, 2, 10, QLatin1Char('0'));
    case Qt::DisplayRole:
        return t->title;
    case Qt::AccessibleTextRole:
        return QStringLiteral("%1 — %2, %3")
            .arg(t->title, t->artist, Theme::formatTime(t->durationMs));
    case Qt::SizeHintRole:
        return QSize(0, 52);
    default:
        return {};
    }
}

Qt::ItemFlags TrackListModel::flags(const QModelIndex &index) const
{
    // Invalid index (root): drop target after last row.
    if (!index.isValid()) {
        Qt::ItemFlags f = Qt::ItemIsDropEnabled;
        return f;
    }
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (m_reorderEnabled)
        f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    return f;
}

QStringList TrackListModel::mimeTypes() const
{
    return {QString::fromLatin1(kMime)};
}

QMimeData *TrackListModel::mimeData(const QModelIndexList &indexes) const
{
    QVector<int> ids;
    for (const QModelIndex &idx : indexes) {
        if (!idx.isValid()) continue;
        const int id = trackIdAt(idx.row());
        if (id > 0 && !ids.contains(id))
            ids.append(id);
    }
    if (ids.isEmpty()) return nullptr;

    auto *mime = new QMimeData;
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream << ids;
    mime->setData(QString::fromLatin1(kMime), bytes);
    return mime;
}

bool TrackListModel::canDropMimeData(const QMimeData *data, Qt::DropAction,
                                     int, int, const QModelIndex &) const
{
    return m_reorderEnabled && data && data->hasFormat(QString::fromLatin1(kMime));
}

Qt::DropActions TrackListModel::supportedDropActions() const
{
    return m_reorderEnabled ? Qt::MoveAction : Qt::IgnoreAction;
}

bool TrackListModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                              const QModelIndex &destinationParent, int destinationChild)
{
    if (sourceParent.isValid() || destinationParent.isValid())
        return false;
    if (count <= 0 || sourceRow < 0 || sourceRow + count > m_ids.size())
        return false;

    int dest = destinationChild;
    if (dest < 0) dest = m_ids.size();
    if (dest > m_ids.size()) dest = m_ids.size();

    // Moving within the same list: adjust dest when removing before insert point.
    if (dest > sourceRow && dest <= sourceRow + count)
        return false; // no-op / invalid
    if (dest > sourceRow)
        dest -= count;

    if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow + count - 1,
                       QModelIndex(), dest > sourceRow ? dest + count : dest)) {
        // Qt's beginMoveRows destination semantics: when moving down, dest is
        // the index after the last moved block in the final layout.
        // Fall back to manual reorder.
        QVector<int> moved;
        for (int i = 0; i < count; ++i)
            moved.append(m_ids[sourceRow + i]);
        for (int i = 0; i < count; ++i)
            m_ids.removeAt(sourceRow);
        int insertAt = destinationChild;
        if (insertAt < 0) insertAt = m_ids.size();
        if (destinationChild > sourceRow)
            insertAt = destinationChild - count;
        insertAt = qBound(0, insertAt, m_ids.size());
        for (int i = 0; i < moved.size(); ++i)
            m_ids.insert(insertAt + i, moved[i]);
        // full reset fallback
        beginResetModel();
        endResetModel();
        persistOrder();
        return true;
    }

    QVector<int> moved;
    for (int i = 0; i < count; ++i)
        moved.append(m_ids[sourceRow + i]);
    for (int i = 0; i < count; ++i)
        m_ids.removeAt(sourceRow);

    int insertAt = destinationChild;
    if (insertAt < 0) insertAt = m_ids.size();
    if (destinationChild > sourceRow)
        insertAt = destinationChild - count;
    insertAt = qBound(0, insertAt, m_ids.size());
    for (int i = 0; i < moved.size(); ++i)
        m_ids.insert(insertAt + i, moved[i]);

    endMoveRows();
    persistOrder();
    return true;
}

bool TrackListModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                  int row, int /*column*/, const QModelIndex &parent)
{
    // InternalMove: perform the move ourselves and return false so the view
    // does NOT call clearOrRemove() on the source rows (Task.md §3.4 / §7.7).
    if (!m_reorderEnabled || !data || !data->hasFormat(QString::fromLatin1(kMime)))
        return false;
    Q_UNUSED(action);

    QByteArray bytes = data->data(QString::fromLatin1(kMime));
    QDataStream stream(&bytes, QIODevice::ReadOnly);
    QVector<int> droppedIds;
    stream >> droppedIds;
    if (droppedIds.isEmpty()) return false;

    // Destination row: -1 / invalid parent means append.
    int destRow = row;
    if (parent.isValid())
        destRow = parent.row();
    if (destRow < 0)
        destRow = m_ids.size();

    // Remove dropped ids from current order (from back to front).
    QVector<int> current = m_ids;
    int firstRemoved = -1;
    for (int id : droppedIds) {
        const int at = current.indexOf(id);
        if (at < 0) continue;
        if (firstRemoved < 0) firstRemoved = at;
        if (at < destRow) destRow--;
        current.removeAt(at);
    }
    destRow = qBound(0, destRow, current.size());
    for (int i = 0; i < droppedIds.size(); ++i) {
        if (!m_ids.contains(droppedIds[i])) continue;
        current.insert(destRow + i, droppedIds[i]);
    }

    beginResetModel();
    m_ids = current;
    endResetModel();
    persistOrder();

    // Return false: we already moved; do not let the view delete source rows.
    return false;
}

void TrackListModel::persistOrder()
{
    if (m_source.kind != Source::Playlist || !m_library)
        return;

    QList<int> ids;
    ids.reserve(m_ids.size());
    for (int id : m_ids)
        ids.append(id);

    if (m_source.playlistId > 0)
        m_library->reorderPlaylist(m_source.playlistId, ids);
    else if (!m_source.playlistName.isEmpty())
        m_library->reorderPlaylist(m_source.playlistName, ids);

    emit orderPersisted();
}
