#ifndef SEARCHPAGE_H
#define SEARCHPAGE_H

#include <QWidget>
#include "trackmodel.h"

class QListView;
class QLabel;
class QTimer;
class QResizeEvent;
class TrackListModel;
class TrackFilterProxy;
class TrackRowDelegate;
class TrackContextMenu;

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

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyQuery();
    void showContext(const QPoint &globalPos);
    QList<int> selectedIds() const;
    void applyResponsiveLayout();

    TrackModel *m_model = nullptr;
    QString m_query;
    QLabel *m_title = nullptr;
    QLabel *m_empty = nullptr;
    QWidget *m_playlistHits = nullptr;
    QListView *m_view = nullptr;
    TrackListModel *m_listModel = nullptr;
    TrackFilterProxy *m_proxy = nullptr;
    TrackRowDelegate *m_delegate = nullptr;
    TrackContextMenu *m_ctx = nullptr;
    QTimer *m_debounce = nullptr;
    int m_currentId = 0;
    bool m_playing = false;
};

#endif // SEARCHPAGE_H
