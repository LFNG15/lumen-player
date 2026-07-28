#ifndef TRACKMODEL_H
#define TRACKMODEL_H

#include <QObject>
#include <QString>
#include <QList>
#include <QUrl>
#include <QDateTime>
#include <QColor>
#include "theme.h"

// Result of adding a track to a playlist (decision 7 — owner notice).
struct AddToPlaylistResult {
    enum Status { Added, AlreadyPresent, Failed } status = Failed;
    bool    crossesOwner = false;  // dest != owner, and owner exists
    bool    firstOwner   = false;  // was standalone; owner just set (file stays put)
    QString trackTitle;
    QString destName;
    QString ownerName;
    QString ownerDirPath;          // on-disk folder of the owner playlist
};

struct Track {
    int     id        = 0;
    QString title;
    QString artist;
    // Display helpers: owner playlist name / id (0 = none). Not unique membership.
    QString folder;
    int     folderId  = 0;
    int     ownerPlaylistId = 0;
    qint64  durationMs = 0;
    int     playCount  = 0;
    Theme::GradientPair cover;
    bool    liked     = false;
    qint64  likedAt   = 0;
    qint64  addedAt   = 0;
    qint64  lastPlayedAt = 0;
    qint64  position  = 0;   // order within a playlist context (from playlist_tracks)
    bool    missing   = false;
    QUrl    audioUrl;

    static Track create(const QString &title, const QString &artist,
                        const QString &folder, const QUrl &url) {
        Track t;
        t.title   = title;
        t.artist  = artist;
        t.folder  = folder;
        t.cover   = Theme::randomPalette();
        t.addedAt = QDateTime::currentMSecsSinceEpoch();
        t.audioUrl = url;
        return t;
    }
};

struct Folder {
    int     id = 0;
    QString name;
    Theme::GradientPair cover;
    QString coverImage;   // absolute path to a cover image; empty = use gradient
    QString dirName;      // on-disk folder name (stable across renames)
    QString sortMode = QStringLiteral("custom");
};

class TrackModel : public QObject {
    Q_OBJECT
public:
    explicit TrackModel(QObject *parent = nullptr);

    QList<Track> &tracks();
    const QList<Track> &tracks() const;

    // Reload library + membership from the database.
    void reload();

    int  addTrack(const Track &track);   // returns the new track's library id
    void removeTrack(int id);
    void updateTrack(int id, const QString &title, const QString &artist);
    void toggleLike(int id);
    void setDuration(int id, qint64 ms);
    void markPlayed(int id);

    Track *findTrack(int id);
    QList<Folder> folders() const;
    QList<Track> tracksInFolder(const QString &folderName) const;
    QList<Track> standaloneTracks() const;
    QList<Track> likedTracks() const;
    QList<Track> recentTracks(int count = 8) const;
    QList<Track> recentlyPlayed(int count = 8) const;
    QList<Folder> recentlyPlayedFolders(int count = 6) const;

    // Playlist CRUD
    int  createPlaylist(const QString &name, const QColor &c1, const QColor &c2,
                        const QString &coverImage = QString());
    void renamePlaylist(int id, const QString &newName);
    void updatePlaylistCover(int id, const QColor &c1, const QColor &c2);
    void updatePlaylistCoverImage(int id, const QString &sourcePath);
    void deletePlaylist(int id);

    // N:N membership (replaces moveTrackToPlaylist).
    AddToPlaylistResult addTrackToPlaylist(int trackId, int playlistId);
    bool removeTrackFromPlaylist(int trackId, int playlistId);
    QList<int> playlistIdsForTrack(int trackId) const;

    void reorderPlaylist(const QString &folderName, const QList<int> &orderedTrackIds);
    void reorderPlaylist(int playlistId, const QList<int> &orderedTrackIds);

    int nextIndex(int currentIndex, bool shuffle) const;
    int prevIndex(int currentIndex) const;

signals:
    void tracksChanged();
    // Fired when a track is added to a second playlist (decision 7).
    void ownerNotice(const AddToPlaylistResult &result);

private:
    QList<Track> m_tracks;
};

#endif // TRACKMODEL_H
