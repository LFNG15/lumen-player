#ifndef QUEUEPAGE_H
#define QUEUEPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QPoint>
#include "trackmodel.h"

class PlayerBar;
class QLabel;
class QScrollArea;
class QResizeEvent;

class QueuePage : public QWidget {
    Q_OBJECT
public:
    explicit QueuePage(TrackModel *model, PlayerBar *player, QWidget *parent = nullptr);
    void refresh(int currentTrackId, bool isPlaying);

signals:
    void playContext(const Track &track);        // play an item, keeping the current context
    void playFromQueue(int index);               // play a manually queued item by index
    void removeFromQueueRequested(int index);    // drop a manually queued item
    void clearQueueRequested();                  // drop all manually queued items
    void reorderQueueRequested(int from, int to); // drag-reorder within the manual queue
    void navigateBack();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QWidget *createRow(const Track &track, const QString &position, bool active,
                       int queueIndex /* -1 if not a manual-queue item */);
    void applyResponsiveLayout();
    int  coverSize() const;
    int  rowHeight() const;

    TrackModel *m_model = nullptr;
    PlayerBar  *m_player = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QLabel *m_headerTitle = nullptr;
    QWidget *m_header = nullptr;
    QScrollArea *m_scroll = nullptr;
    int m_lastCurrentId = 0;
    bool m_lastPlaying = false;
    QPoint m_dragStartPos;
};

#endif // QUEUEPAGE_H
