#ifndef LUMEN_TRACKLISTMODEL_H
#define LUMEN_TRACKLISTMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QString>
#include "trackmodel.h"

class TrackListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        DurationRole,
        Color1Role,
        Color2Role,
        LikedRole,
        PlayCountRole,
        AddedAtRole,
        LastPlayedRole,
        OwnerNameRole,
        MissingRole,
        IsCurrentRole,
        IsPlayingRole,
        SearchKeyRole,
        PositionRole,
        IndexLabelRole,
    };

    struct Source {
        enum Kind { All, Playlist, Liked, Standalone, RecentlyAdded, RecentlyPlayed } kind = All;
        int     playlistId = 0;
        QString playlistName; // for reorder persistence by name
        int     limit = 0;    // RecentlyAdded/RecentlyPlayed only; 0 falls back to TrackModel's default (8)
    };

    explicit TrackListModel(TrackModel *library, QObject *parent = nullptr);

    void setSource(Source src);
    Source source() const { return m_source; }

    void setPlaybackState(int currentTrackId, bool isPlaying);
    void setReorderEnabled(bool on);
    bool reorderEnabled() const { return m_reorderEnabled; }

    // Rebuild id vector from TrackModel according to source + optional pre-sorted ids.
    void reload();
    // Apply an explicit ordered id list (e.g. after client-side sort).
    void setOrderedIds(const QVector<int> &ids);

    int trackIdAt(int row) const;
    QList<Track> tracksInOrder() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // DnD
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent) override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action,
                         int row, int column, const QModelIndex &parent) const override;
    Qt::DropActions supportedDropActions() const override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destinationParent, int destinationChild) override;

    static const char *mimeType();

signals:
    void orderPersisted();

private:
    TrackModel *m_library = nullptr;
    Source m_source;
    QVector<int> m_ids;
    int  m_currentId = 0;
    bool m_playing = false;
    bool m_reorderEnabled = false;

    void persistOrder();
};

#endif // LUMEN_TRACKLISTMODEL_H
