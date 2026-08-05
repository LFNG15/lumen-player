#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include "trackmodel.h"
#include "models/tracklistmodel.h"

class TrackContextMenu;
class QListView;
class QLabel;

class HomePage : public QWidget {
    Q_OBJECT
public:
    explicit HomePage(TrackModel *model, QWidget *parent = nullptr);
    void refresh(int currentTrackId, bool isPlaying);

signals:
    void playRequested(const Track &track);
    void likeToggled(int id);
    void enqueueRequested(const Track &track);
    void editTrackRequested(const Track &track);
    void deleteRequested(int id);
    void navigateTo(const QString &page, const QString &data = "");

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    // One QListView+TrackListModel+TrackRowDelegate, wired identically to how
    // LikedPage/FolderDetailPage/SearchPage already do it. limit > 0 makes a
    // non-scrolling "shelf" (height follows content, capped item count);
    // limit == 0 is a normal unbounded, independently-scrolling list.
    QWidget *buildShelf(const QString &labelText, TrackListModel::Source::Kind kind,
                        int limit, bool allowDelete,
                        QListView **outView, TrackListModel **outModel);
    void updateShelfHeight(QListView *view, TrackListModel *model);
    QList<int> selectedIds(QListView *view, TrackListModel *model) const;

    QWidget *createFolderChip(const Folder &folder, int trackCount, int chipWidth);
    QWidget *createChipCover(const Folder &folder);
    QWidget *createRecentCard(const Folder &folder);
    int chipColumnsForWidth(int w) const;

    TrackModel *m_model;
    TrackContextMenu *m_ctx = nullptr;

    // Top region: greeting/chips/"Recentes" strip (dynamic, rebuilt each
    // refresh()) followed by the two capped shelves (persistent, just reload()).
    QScrollArea *m_scroll = nullptr;
    QWidget *m_dynamicRegion = nullptr;
    QVBoxLayout *m_dynamicLayout = nullptr;

    QWidget *m_playedSection = nullptr;
    QListView *m_playedView = nullptr;
    TrackListModel *m_playedModel = nullptr;

    QWidget *m_addedSection = nullptr;
    QListView *m_addedView = nullptr;
    TrackListModel *m_addedModel = nullptr;

    // Bottom region: "Biblioteca Completa" — unbounded, own independent scroll
    // (not nested inside m_scroll — see homepage.cpp for why).
    QWidget *m_librarySection = nullptr;
    QListView *m_libraryView = nullptr;
    TrackListModel *m_libraryModel = nullptr;

    int m_lastChipCols = -1;
    int m_lastCurrentId = 0;
    bool m_lastPlaying = false;
};

#endif // HOMEPAGE_H
