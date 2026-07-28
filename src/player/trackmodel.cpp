#include "trackmodel.h"
#include "database.h"
#include "mediatools.h"
#include <QRandomGenerator>
#include <QPair>
#include <algorithm>

TrackModel::TrackModel(QObject *parent) : QObject(parent) {
    Database::instance().open();
    m_tracks = Database::instance().allTracks();
}

QList<Track> &TrackModel::tracks() { return m_tracks; }
const QList<Track> &TrackModel::tracks() const { return m_tracks; }

int TrackModel::addTrack(const Track &track) {
    int folderId = 0;
    if (!track.folder.isEmpty()) {
        folderId = Database::instance().findOrCreateFolder(
            track.folder, track.cover.c1, track.cover.c2);
        // Keep a matching folder on disk under the downloads root so the
        // playlist's files are easy to locate.
        MediaTools::playlistDir(track.folder);
    }

    int newId = Database::instance().insertTrack(track, folderId);

    Track t    = track;
    t.id       = newId;
    t.folderId = folderId;
    m_tracks.prepend(t);
    emit tracksChanged();
    return newId;
}

void TrackModel::removeTrack(int id) {
    Database::instance().deleteTrack(id);
    m_tracks.erase(std::remove_if(m_tracks.begin(), m_tracks.end(),
        [id](const Track &t) { return t.id == id; }), m_tracks.end());
    emit tracksChanged();
}

void TrackModel::updateTrack(int id, const QString &title, const QString &artist) {
    QString finalArtist = artist.isEmpty() ? QStringLiteral("Desconhecido") : artist;
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

void TrackModel::toggleLike(int id) {
    for (auto &t : m_tracks) {
        if (t.id == id) {
            t.liked = !t.liked;
            Database::instance().setLiked(id, t.liked);
            emit tracksChanged();
            return;
        }
    }
}

void TrackModel::markPlayed(int id) {
    Database::instance().markPlayed(id);
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto &t : m_tracks) {
        if (t.id == id) {
            t.lastPlayedAt = now;
            t.playCount++;
            break;
        }
    }
    emit tracksChanged();
}

void TrackModel::setDuration(int id, qint64 ms) {
    for (auto &t : m_tracks) {
        if (t.id == id) {
            t.durationMs = ms;
            Database::instance().setDuration(id, ms);
            return;
        }
    }
}

Track *TrackModel::findTrack(int id) {
    for (auto &t : m_tracks) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

QList<Folder> TrackModel::folders() const {
    return Database::instance().allFolders();
}

QList<Track> TrackModel::tracksInFolder(const QString &folderName) const {
    QList<Track> result;
    for (auto &t : m_tracks) {
        if (t.folder == folderName) result.append(t);
    }
    // Honour the playlist's custom order. Tracks added in the same batch can
    // share the same timestamp position, so break ties by id — otherwise the
    // unstable sort reshuffles them on every refresh.
    std::sort(result.begin(), result.end(),
        [](const Track &a, const Track &b) {
            return a.position != b.position ? a.position < b.position
                                            : a.id < b.id;
        });
    return result;
}

QList<Track> TrackModel::standaloneTracks() const {
    QList<Track> result;
    for (auto &t : m_tracks) {
        if (t.folderId == 0 && t.folder.isEmpty()) result.append(t);
    }
    return result;
}

QList<Track> TrackModel::likedTracks() const {
    QList<Track> result;
    for (auto &t : m_tracks) {
        if (t.liked) result.append(t);
    }
    return result;
}

QList<Track> TrackModel::recentTracks(int count) const {
    QList<Track> sorted = m_tracks;
    std::sort(sorted.begin(), sorted.end(),
        [](const Track &a, const Track &b) { return a.addedAt > b.addedAt; });
    return sorted.mid(0, count);
}

QList<Track> TrackModel::recentlyPlayed(int count) const {
    QList<Track> played;
    for (const auto &t : m_tracks)
        if (t.lastPlayedAt > 0) played.append(t);
    std::sort(played.begin(), played.end(),
        [](const Track &a, const Track &b) { return a.lastPlayedAt > b.lastPlayedAt; });
    return played.mid(0, count);
}

QList<Folder> TrackModel::recentlyPlayedFolders(int count) const {
    QList<QPair<qint64, Folder>> played;
    for (const auto &f : folders()) {
        qint64 last = 0;
        for (const auto &t : m_tracks)
            if (t.folderId == f.id && t.lastPlayedAt > last) last = t.lastPlayedAt;
        if (last > 0) played.append({last, f});
    }
    std::sort(played.begin(), played.end(),
        [](const QPair<qint64, Folder> &a, const QPair<qint64, Folder> &b) {
            return a.first > b.first;
        });
    QList<Folder> result;
    for (int i = 0; i < played.size() && i < count; ++i) result.append(played[i].second);
    return result;
}

int TrackModel::createPlaylist(const QString &name, const QColor &c1, const QColor &c2,
                               const QString &coverImage) {
    QString stored = Database::importCoverImage(coverImage);
    int id = Database::instance().createFolder(name, c1, c2, stored);
    // Every playlist gets a matching folder under the downloads root.
    MediaTools::playlistDir(name);
    emit tracksChanged();
    return id;
}

void TrackModel::renamePlaylist(int id, const QString &newName) {
    Database::instance().renameFolder(id, newName);
    for (auto &t : m_tracks) {
        if (t.folderId == id) t.folder = newName;
    }
    emit tracksChanged();
}

void TrackModel::updatePlaylistCover(int id, const QColor &c1, const QColor &c2) {
    Database::instance().updateFolderCover(id, c1, c2);
    emit tracksChanged();
}

void TrackModel::updatePlaylistCoverImage(int id, const QString &sourcePath) {
    // Empty path clears the image and reverts to the gradient cover.
    QString stored = Database::importCoverImage(sourcePath);
    Database::instance().updateFolderCoverImage(id, stored);
    emit tracksChanged();
}

void TrackModel::deletePlaylist(int id) {
    Database::instance().deleteFolder(id);
    for (auto &t : m_tracks) {
        if (t.folderId == id) {
            t.folderId = 0;
            t.folder   = "";
        }
    }
    emit tracksChanged();
}

void TrackModel::moveTrackToPlaylist(int trackId, int playlistId, const QString &playlistName) {
    Database::instance().moveTrackToFolder(trackId, playlistId);
    for (auto &t : m_tracks) {
        if (t.id == trackId) {
            t.folderId = playlistId;
            t.folder   = playlistName;
        }
    }
    emit tracksChanged();
}

void TrackModel::reorderPlaylist(const QString &folderName, const QList<int> &orderedTrackIds) {
    Q_UNUSED(folderName);
    // Gap scale: rank 0 → 1024, rank 1 → 2048, … so 0 is never a valid
    // position (Issue #2). Single transaction via setTrackPositions.
    QList<QPair<int, qint64>> positions;
    positions.reserve(orderedTrackIds.size());
    for (int i = 0; i < orderedTrackIds.size(); ++i) {
        const qint64 pos = static_cast<qint64>(i + 1) * 1024;
        positions.append(qMakePair(orderedTrackIds[i], pos));
        for (auto &t : m_tracks)
            if (t.id == orderedTrackIds[i]) { t.position = pos; break; }
    }
    Database::instance().setTrackPositions(positions);
    emit tracksChanged();
}

int TrackModel::nextIndex(int currentIndex, bool shuffle) const {
    if (m_tracks.isEmpty()) return -1;
    if (shuffle) return QRandomGenerator::global()->bounded(m_tracks.size());
    return (currentIndex + 1) % m_tracks.size();
}

int TrackModel::prevIndex(int currentIndex) const {
    if (m_tracks.isEmpty()) return -1;
    return (currentIndex - 1 + m_tracks.size()) % m_tracks.size();
}
