#ifndef QUEUEPAGE_H
#define QUEUEPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include "trackmodel.h"

class PlayerBar;

class QueuePage : public QWidget {
    Q_OBJECT
public:
    explicit QueuePage(TrackModel *model, PlayerBar *player, QWidget *parent = nullptr);
    void refresh(int currentTrackId, bool isPlaying);

signals:
    void playContext(const Track &track);        // play an item, keeping the current context
    void playFromQueue(int index);               // play a manually queued item by index
    void removeFromQueueRequested(int index);    // drop a manually queued item
    void navigateBack();

private:
    QWidget *createRow(const Track &track, const QString &position, bool active,
                       int queueIndex /* -1 if not a manual-queue item */);

    TrackModel *m_model;
    PlayerBar  *m_player;
    QVBoxLayout *m_contentLayout;
};

#endif // QUEUEPAGE_H
