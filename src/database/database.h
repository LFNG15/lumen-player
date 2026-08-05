#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QList>
#include <QPair>
#include <QColor>
#include "trackmodel.h"

class Database {
public:
    static Database &instance();

    bool open();
    QString lastError() const { return m_lastError; }

    // --- Playlists (table: playlists) ---
    int  findOrCreatePlaylist(const QString &name, const QColor &c1, const QColor &c2);
    int  createPlaylist(const QString &name, const QColor &c1, const QColor &c2,
                        const QString &coverImage = QString());
    QList<Folder> allPlaylists();
    Folder playlistById(int id);
    Folder playlistByName(const QString &name);
    void renamePlaylist(int id, const QString &newName);  // does NOT change dir_name
    void updatePlaylistCover(int id, const QColor &c1, const QColor &c2);
    void updatePlaylistCoverImage(int id, const QString &imagePath);
    void deletePlaylist(int id);  // CASCADE membership; owner_playlist_id → NULL
    void setPlaylistSortMode(int id, const QString &mode);
    QString playlistSortMode(int id) const;
    // Absolute on-disk directory for the playlist (downloadDir/dir_name).
    QString playlistDiskPath(int id) const;
    QString playlistDiskPathByName(const QString &name) const;

    // Legacy aliases used by older call sites during the transition.
    int  findOrCreateFolder(const QString &name, const QColor &c1, const QColor &c2)
    { return findOrCreatePlaylist(name, c1, c2); }
    int  createFolder(const QString &name, const QColor &c1, const QColor &c2,
                      const QString &coverImage = QString())
    { return createPlaylist(name, c1, c2, coverImage); }
    QList<Folder> allFolders() { return allPlaylists(); }
    void renameFolder(int id, const QString &newName) { renamePlaylist(id, newName); }
    void updateFolderCover(int id, const QColor &c1, const QColor &c2)
    { updatePlaylistCover(id, c1, c2); }
    void updateFolderCoverImage(int id, const QString &imagePath)
    { updatePlaylistCoverImage(id, imagePath); }
    void deleteFolder(int id) { deletePlaylist(id); }

    static QString importCoverImage(const QString &sourcePath);

    // --- Tracks ---
    // Insert a library track. If playlistId > 0, also adds membership and may
    // set owner_playlist_id when the track is new.
    int  insertTrack(const Track &t, int playlistId = 0);
    void updateTrack(int id, const QString &title, const QString &artist);
    void deleteTrack(int id);
    void setLiked(int id, bool liked);
    void setDuration(int id, qint64 ms);
    void markPlayed(int id);
    void incrementPlayCount(int id);
    QList<Track> allTracks();
    QList<Track> tracksInPlaylist(int playlistId);
    QList<Track> tracksInPlaylistByName(const QString &name);
    QList<int>  playlistIdsForTrack(int trackId);

    // --- Membership (playlist_tracks) ---
    AddToPlaylistResult addTrackToPlaylist(int trackId, int playlistId);
    bool removeTrackFromPlaylist(int trackId, int playlistId);
    // Batch-write positions for one playlist (gap scale). pairs: (trackId, position)
    void setPlaylistTrackPositions(int playlistId,
                                   const QList<QPair<int, qint64>> &positions);
    qint64 nextPositionInPlaylist(int playlistId);
    void normalisePlaylistPositions(int playlistId);

    // Debug / CI
    int seedFakeLibrary(int n);

    struct PlaybackState {
        int    trackId    = 0;
        qint64 posMs      = 0;
        double volume     = 0.7;
        bool   muted      = false;
        bool   shuffle    = false;
        int    repeatMode = 0;   // 0=Off, 1=All, 2=One
        QList<int> contextIds;
        QList<int> userQueueIds;
        int     contextIndex = -1;  // position within contextIds when a manual-queue
                                     // track was playing (see PlaybackEngine::m_contextIndex)
        QString contextName;
    };
    PlaybackState loadState();
    void          saveState(const PlaybackState &s);

private:
    Database() = default;
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    void  applySchema();
    void  ensureLegacySchema();
    void  ensureV2Schema();
    void  ensurePlaybackStateSchema();
    bool  tableExists(const QString &name) const;
    Track rowToTrack(const class QSqlQuery &q, int ownerNameCol = -1,
                     int positionCol = -1) const;

    QString m_lastError;
};

#endif // DATABASE_H
