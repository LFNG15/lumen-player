#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QList>
#include <QPair>
#include "trackmodel.h"

class Database {
public:
    static Database &instance();

    bool open();
    QString lastError() const { return m_lastError; }

    int  findOrCreateFolder(const QString &name, const QColor &c1, const QColor &c2);
    int  createFolder(const QString &name, const QColor &c1, const QColor &c2,
                      const QString &coverImage = QString());
    QList<Folder> allFolders();
    void renameFolder(int id, const QString &newName);
    void updateFolderCover(int id, const QColor &c1, const QColor &c2);
    void updateFolderCoverImage(int id, const QString &imagePath);
    void deleteFolder(int id);

    // Copies a chosen image into the app's covers/ directory and returns the
    // stored path (empty input yields an empty result).
    static QString importCoverImage(const QString &sourcePath);

    int  insertTrack(const Track &t, int folderId);
    void updateTrack(int id, const QString &title, const QString &artist);
    void deleteTrack(int id);
    void setLiked(int id, bool liked);
    void setDuration(int id, qint64 ms);
    void markPlayed(int id);          // bumps play_count and last_played_at
    void incrementPlayCount(int id);
    void setTrackPosition(int id, qint64 position);
    // Batch update of positions inside a single transaction (gap scale).
    void setTrackPositions(const QList<QPair<int, qint64>> &positions);
    // Next append position for a folder: COALESCE(MAX(position),0) + 1024.
    qint64 nextPositionForFolder(int folderId);
    void moveTrackToFolder(int trackId, int folderId);
    QList<Track> allTracks();

    // Debug / CI helpers (P0.b)
    // Populate the open DB with N synthetic tracks across a few playlists.
    // Returns the number of tracks inserted. No-op if N <= 0.
    int seedFakeLibrary(int n);

    struct PlaybackState {
        int    trackId  = 0;
        qint64 posMs    = 0;
        double volume   = 0.7;
        bool   shuffle  = false;
        bool   repeat   = false;
    };
    PlaybackState loadState();
    void          saveState(const PlaybackState &s);

private:
    Database() = default;
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    void  applySchema();
    Track rowToTrack(const class QSqlQuery &q) const;

    QString m_lastError;
};

#endif // DATABASE_H
