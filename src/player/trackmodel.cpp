#include "trackmodel.h"
#include "database.h"
#include "mediatools.h"
#include <QRandomGenerator>
#include <QPair>
#include <QSet>
#include <algorithm>

TrackModel::TrackModel(QObject *parent) : QObject(parent)
{
    Database::instance().open();
    reload();
}

void TrackModel::reload()
{
    m_tracks = Database::instance().allTracks();
}

QList<Track> &TrackModel::tracks() { return m_tracks; }
const QList<Track> &TrackModel::tracks() const { return m_tracks; }

int TrackModel::addTrack(const Track &track)
{
    int playlistId = 0;
    if (!track.folder.isEmpty()) {
        playlistId = Database::instance().findOrCreatePlaylist(
            track.folder, track.cover.c1, track.cover.c2);
        // Ensure on-disk folder exists (uses dir_name once the row exists).
        Database::instance().playlistDiskPath(playlistId);
    }

    const int newId = Database::instance().insertTrack(track, playlistId);
    reload();
    emit tracksChanged();
    return newId;
}

void TrackModel::removeTrack(int id)
{
    Database::instance().deleteTrack(id);
    m_tracks.erase(std::remove_if(m_tracks.begin(), m_tracks.end(),
        [id](const Track &t) { return t.id == id; }), m_tracks.end());
    emit tracksChanged();
}

void TrackModel::updateTrack(int id, const QString &title, const QString &artist)
{
    const QString finalArtist = artist.isEmpty() ? QStringLiteral("Desconhecido") : artist;
    Database::instance().updateTrack(id, title, finalArtist);
    for (auto &t : m_tracks) {
        if (t.id == id) {
            t.title  = title;
            t.artist = finalArtist;
            break;
        }
    }
    emit tracksChanged();
}

void TrackModel::toggleLike(int id)
{
    for (auto &t : m_tracks) {
        if (t.id == id) {
            t.liked = !t.liked;
            t.likedAt = t.liked ? QDateTime::currentMSecsSinceEpoch() : 0;
            Database::instance().setLiked(id, t.liked);
            emit tracksChanged();
            return;
        }
    }
}

void TrackModel::markPlayed(int id)
{
    Database::instance().markPlayed(id);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto &t : m_tracks) {
        if (t.id == id) {
            t.lastPlayedAt = now;
            t.playCount++;
            break;
        }
    }
    emit tracksChanged();
}

void TrackModel::setDuration(int id, qint64 ms)
{
    for (auto &t : m_tracks) {
        if (t.id == id) {
            t.durationMs = ms;
            Database::instance().setDuration(id, ms);
            return;
        }
    }
}

Track *TrackModel::findTrack(int id)
{
    for (auto &t : m_tracks) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

QList<Folder> TrackModel::folders() const
{
    return Database::instance().allPlaylists();
}

QList<Track> TrackModel::tracksInFolder(const QString &folderName) const
{
    return Database::instance().tracksInPlaylistByName(folderName);
}

QList<Track> TrackModel::standaloneTracks() const
{
    // Tracks with no playlist membership.
    QSet<int> inAny;
    for (const auto &f : folders()) {
        for (const auto &t : Database::instance().tracksInPlaylist(f.id))
            inAny.insert(t.id);
    }
    QList<Track> result;
    for (const auto &t : m_tracks) {
        if (!inAny.contains(t.id))
            result.append(t);
    }
    return result;
}

QList<Track> TrackModel::likedTracks() const
{
    QList<Track> result;
    for (const auto &t : m_tracks) {
        if (t.liked) result.append(t);
    }
    std::sort(result.begin(), result.end(),
        [](const Track &a, const Track &b) {
            return a.likedAt != b.likedAt ? a.likedAt > b.likedAt
                                          : a.id > b.id;
        });
    return result;
}

QList<Track> TrackModel::recentTracks(int count) const
{
    QList<Track> sorted = m_tracks;
    std::sort(sorted.begin(), sorted.end(),
        [](const Track &a, const Track &b) { return a.addedAt > b.addedAt; });
    return sorted.mid(0, count);
}

QList<Track> TrackModel::recentlyPlayed(int count) const
{
    QList<Track> played;
    for (const auto &t : m_tracks)
        if (t.lastPlayedAt > 0) played.append(t);
    std::sort(played.begin(), played.end(),
        [](const Track &a, const Track &b) { return a.lastPlayedAt > b.lastPlayedAt; });
    return played.mid(0, count);
}

QList<Folder> TrackModel::recentlyPlayedFolders(int count) const
{
    QList<QPair<qint64, Folder>> played;
    for (const auto &f : folders()) {
        qint64 last = 0;
        for (const auto &t : Database::instance().tracksInPlaylist(f.id))
            if (t.lastPlayedAt > last) last = t.lastPlayedAt;
        if (last > 0) played.append({last, f});
    }
    std::sort(played.begin(), played.end(),
        [](const QPair<qint64, Folder> &a, const QPair<qint64, Folder> &b) {
            return a.first > b.first;
        });
    QList<Folder> result;
    for (int i = 0; i < played.size() && i < count; ++i)
        result.append(played[i].second);
    return result;
}

int TrackModel::createPlaylist(const QString &name, const QColor &c1, const QColor &c2,
                               const QString &coverImage)
{
    const QString stored = Database::importCoverImage(coverImage);
    const int id = Database::instance().createPlaylist(name, c1, c2, stored);
    if (id > 0)
        Database::instance().playlistDiskPath(id); // ensure dir exists
    emit tracksChanged();
    return id;
}

void TrackModel::renamePlaylist(int id, const QString &newName)
{
    Database::instance().renamePlaylist(id, newName);
    for (auto &t : m_tracks) {
        if (t.ownerPlaylistId == id || t.folderId == id)
            t.folder = newName;
    }
    emit tracksChanged();
}

void TrackModel::updatePlaylistCover(int id, const QColor &c1, const QColor &c2)
{
    Database::instance().updatePlaylistCover(id, c1, c2);
    emit tracksChanged();
}

void TrackModel::updatePlaylistCoverImage(int id, const QString &sourcePath)
{
    const QString stored = Database::importCoverImage(sourcePath);
    Database::instance().updatePlaylistCoverImage(id, stored);
    emit tracksChanged();
}

void TrackModel::deletePlaylist(int id)
{
    Database::instance().deletePlaylist(id);
    reload();
    emit tracksChanged();
}

AddToPlaylistResult TrackModel::addTrackToPlaylist(int trackId, int playlistId)
{
    const AddToPlaylistResult r =
        Database::instance().addTrackToPlaylist(trackId, playlistId);
    if (r.status == AddToPlaylistResult::Added) {
        reload();
        emit tracksChanged();
        if (r.crossesOwner || r.firstOwner)
            emit ownerNotice(r);
    }
    return r;
}

bool TrackModel::removeTrackFromPlaylist(int trackId, int playlistId)
{
    if (!Database::instance().removeTrackFromPlaylist(trackId, playlistId))
        return false;
    reload();
    emit tracksChanged();
    return true;
}

QList<int> TrackModel::playlistIdsForTrack(int trackId) const
{
    return Database::instance().playlistIdsForTrack(trackId);
}

void TrackModel::reorderPlaylist(const QString &folderName,
                                 const QList<int> &orderedTrackIds)
{
    const Folder f = Database::instance().playlistByName(folderName);
    if (f.id > 0)
        reorderPlaylist(f.id, orderedTrackIds);
}

void TrackModel::reorderPlaylist(int playlistId, const QList<int> &orderedTrackIds)
{
    if (playlistId <= 0) return;

    QList<QPair<int, qint64>> positions;
    positions.reserve(orderedTrackIds.size());
    for (int i = 0; i < orderedTrackIds.size(); ++i) {
        const qint64 pos = static_cast<qint64>(i + 1) * 1024;
        positions.append(qMakePair(orderedTrackIds[i], pos));
    }
    Database::instance().setPlaylistTrackPositions(playlistId, positions);
    emit tracksChanged();
}

int TrackModel::nextIndex(int currentIndex, bool shuffle) const
{
    if (m_tracks.isEmpty()) return -1;
    if (shuffle) return QRandomGenerator::global()->bounded(m_tracks.size());
    return (currentIndex + 1) % m_tracks.size();
}

int TrackModel::prevIndex(int currentIndex) const
{
    if (m_tracks.isEmpty()) return -1;
    return (currentIndex - 1 + m_tracks.size()) % m_tracks.size();
}
