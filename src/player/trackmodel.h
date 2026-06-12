#ifndef TRACKMODEL_H
#define TRACKMODEL_H

#include <QObject>
#include <QString>
#include <QList>
#include <QUrl>
#include <QDateTime>
#include <QColor>
#include "theme.h"

struct Track {
    int     id        = 0;
    QString title;
    QString artist;
    QString folder;
    int     folderId  = 0;
    qint64  durationMs = 0;
    int     playCount  = 0;
    Theme::GradientPair cover;
    bool    liked     = false;
    qint64  addedAt   = 0;
    qint64  lastPlayedAt = 0;
    qint64  position  = 0;   // order within its playlist
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
};

class TrackModel : public QObject {
    Q_OBJECT
public:
    explicit TrackModel(QObject *parent = nullptr);

    QList<Track> &tracks();
    const QList<Track> &tracks() const;

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
    // Playlists ordered by when one of their tracks was last played.
    QList<Folder> recentlyPlayedFolders(int count = 6) const;

    // Playlist CRUD
    int  createPlaylist(const QString &name, const QColor &c1, const QColor &c2,
                        const QString &coverImage = QString());
    void renamePlaylist(int id, const QString &newName);
    void updatePlaylistCover(int id, const QColor &c1, const QColor &c2);
    void updatePlaylistCoverImage(int id, const QString &sourcePath);
    void deletePlaylist(int id);
    void moveTrackToPlaylist(int trackId, int playlistId, const QString &playlistName);
    void reorderPlaylist(const QString &folderName, const QList<int> &orderedTrackIds);

    int nextIndex(int currentIndex, bool shuffle) const;
    int prevIndex(int currentIndex) const;

signals:
    void tracksChanged();

private:
    QList<Track> m_tracks;
};

#endif // TRACKMODEL_H
