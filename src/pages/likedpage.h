#ifndef LIKEDPAGE_H
#define LIKEDPAGE_H

#include <QWidget>
#include "trackmodel.h"

class QListView;
class TrackListModel;
class TrackRowDelegate;
class TrackContextMenu;

class LikedPage : public QWidget {
    Q_OBJECT
public:
    explicit LikedPage(TrackModel *model, QWidget *parent = nullptr);
    void refresh(int currentTrackId, bool isPlaying);

signals:
    void playRequested(const Track &track);
    void likeToggled(int id);
    void enqueueRequested(const Track &track);
    void editTrackRequested(const Track &track);
    void deleteRequested(int id);
    void navigateBack();
    void navigateToFolder(const QString &folderName);

private:
    void rebuildHeader();
    void showContext(const QPoint &globalPos);
    QList<int> selectedIds() const;

    TrackModel *m_model = nullptr;
    QWidget *m_header = nullptr;
    QListView *m_view = nullptr;
    TrackListModel *m_listModel = nullptr;
    TrackRowDelegate *m_delegate = nullptr;
    TrackContextMenu *m_ctx = nullptr;
};

#endif // LIKEDPAGE_H
