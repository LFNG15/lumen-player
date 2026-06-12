#ifndef FOLDERDETAILPAGE_H
#define FOLDERDETAILPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include "trackmodel.h"

class QListWidget;
class QListWidgetItem;

class FolderDetailPage : public QWidget {
    Q_OBJECT
public:
    explicit FolderDetailPage(TrackModel *model, QWidget *parent = nullptr);
    void setFolder(const QString &folderName);
    void refresh(int currentTrackId, bool isPlaying);

    // Tracks in the order currently shown (after the sort mode is applied) —
    // used as the playback queue so next/prev follow what the user sees.
    QList<Track> displayedTracks() const { return m_displayedTracks; }

signals:
    void playRequested(const Track &track);
    void likeToggled(int id);
    void deleteRequested(int id);
    void enqueueRequested(const Track &track);
    void editTrackRequested(const Track &track);
    void navigateBack();

private:
    void showEditDialog();
    void showMoveDialog(int trackId);
    void showCoverLightbox(const QString &imagePath);
    void applyFilter();
    QString sortMode() const;
    void showSortMenu();

    TrackModel *m_model;
    QString m_folderName;
    int m_folderId = 0;
    QVBoxLayout *m_contentLayout;
    QListWidget *m_trackList = nullptr;

    QString m_filterText;            // in-playlist search, survives refreshes
    QList<Track> m_displayedTracks;  // tracks in the displayed (sorted) order
    bool m_canReorder = false;       // drag-reorder allowed (custom sort only)
    int  m_lastCurrentId = 0;        // last refresh args, for internal reloads
    bool m_lastPlaying = false;
};

#endif // FOLDERDETAILPAGE_H
