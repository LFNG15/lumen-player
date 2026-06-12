#ifndef SEARCHPAGE_H
#define SEARCHPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include "trackmodel.h"

// Global search results: playlists and tracks matching the query typed in the
// top search bar (title, artist or playlist name, case/accent-insensitive).
class SearchPage : public QWidget {
    Q_OBJECT
public:
    explicit SearchPage(TrackModel *model, QWidget *parent = nullptr);

    void setQuery(const QString &query);
    QString query() const { return m_query; }
    void refresh(int currentTrackId, bool isPlaying);

signals:
    void playRequested(const Track &track);
    void likeToggled(int id);
    void enqueueRequested(const Track &track);
    void navigateTo(const QString &page, const QString &data = "");

private:
    QWidget *createTrackRow(const Track &track, int index, int currentId);
    QWidget *createPlaylistCard(const Folder &folder, int trackCount);

    TrackModel *m_model;
    QVBoxLayout *m_contentLayout;
    QString m_query;
};

#endif // SEARCHPAGE_H
