#ifndef FOLDERDETAILPAGE_H
#define FOLDERDETAILPAGE_H

#include <QWidget>
#include "trackmodel.h"

class QListView;
class QLineEdit;
class QLabel;
class QPushButton;
class TrackListModel;
class TrackFilterProxy;
class TrackRowDelegate;
class TrackContextMenu;

// Playlist detail with virtualized track list (P3). Header is fixed; the
// QListView owns scrolling — never setFixedHeight(n * rowHeight).
class FolderDetailPage : public QWidget {
    Q_OBJECT
public:
    explicit FolderDetailPage(TrackModel *model, QWidget *parent = nullptr);
    void setFolder(const QString &folderName);
    void refresh(int currentTrackId, bool isPlaying);

    QList<Track> displayedTracks() const;

signals:
    void playRequested(const Track &track);
    void likeToggled(int id);
    void deleteRequested(int id);
    void enqueueRequested(const Track &track);
    void editTrackRequested(const Track &track);
    void navigateBack();

private:
    void setupHeaderUi();
    void updateHeader();
    void replaceCover(QWidget *cover);
    void applySortToModel();
    void updateReorderFlag();
    void showEditDialog();
    void showCoverLightbox(const QString &imagePath);
    void showSortMenu();
    void showContextForSelection(const QPoint &globalPos);
    QString sortMode() const;
    QList<int> selectedTrackIds() const;

    TrackModel *m_model = nullptr;
    QString m_folderName;
    int m_folderId = 0;

    QWidget *m_header = nullptr;
    QWidget *m_coverHost = nullptr;
    QLabel *m_typeLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_statsLabel = nullptr;
    QPushButton *m_editBtn = nullptr;
    QPushButton *m_playBtn = nullptr;
    QListView *m_view = nullptr;
    TrackListModel *m_listModel = nullptr;
    TrackFilterProxy *m_proxy = nullptr;
    TrackRowDelegate *m_delegate = nullptr;
    TrackContextMenu *m_ctx = nullptr;
    QLineEdit *m_searchEdit = nullptr;

    QString m_filterText;
    int  m_lastCurrentId = 0;
    bool m_lastPlaying = false;
    bool m_canReorder = false;
};

#endif // FOLDERDETAILPAGE_H
