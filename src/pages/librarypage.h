#ifndef LIBRARYPAGE_H
#define LIBRARYPAGE_H

#include <QWidget>
#include "trackmodel.h"

class QListView;
class QLabel;
class TrackListModel;
class TrackRowDelegate;
class TrackContextMenu;

// Top-level sidebar destination — the full, unbounded library as its own
// page (previously the "Biblioteca Completa" section embedded in HomePage).
class LibraryPage : public QWidget {
    Q_OBJECT
public:
    explicit LibraryPage(TrackModel *model, QWidget *parent = nullptr);
    void refresh(int currentTrackId, bool isPlaying);

signals:
    void playRequested(const Track &track);
    void likeToggled(int id);
    void enqueueRequested(const Track &track);
    void editTrackRequested(const Track &track);
    void deleteRequested(int id);
    void navigateBack();

private:
    void showContext(const QPoint &globalPos);
    QList<int> selectedIds() const;

    TrackModel *m_model = nullptr;
    QWidget *m_header = nullptr;
    QLabel *m_countLabel = nullptr;
    QListView *m_view = nullptr;
    TrackListModel *m_listModel = nullptr;
    TrackRowDelegate *m_delegate = nullptr;
    TrackContextMenu *m_ctx = nullptr;
};

#endif // LIBRARYPAGE_H
