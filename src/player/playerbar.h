#ifndef PLAYERBAR_H
#define PLAYERBAR_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include "clickableslider.h"
#include "vinylwidget.h"
#include "playbackengine.h"

// Pure view over PlaybackEngine (P4). Transport logic lives in the engine.
class PlayerBar : public QWidget {
    Q_OBJECT
public:
    explicit PlayerBar(PlaybackEngine *engine, QWidget *parent = nullptr);

    PlaybackEngine *engine() const { return m_engine; }

    // Thin facades so existing MainWindow/QueuePage call sites keep working.
    void playTrack(const Track &track, const QList<Track> &queue = QList<Track>(),
                    const QString &contextName = QString());
    void playKeepingContext(const Track &track);
    void togglePlay();
    void next();
    void prev();
    void enqueue(const Track &track);
    void removeFromQueue(int index);
    bool takeFromQueue(int index, Track &out);
    void clearUserQueue();
    void reorderUserQueue(int from, int to);
    QList<Track> userQueue() const;
    QList<Track> upcomingContext() const;
    QString contextName() const;
    Track currentTrack() const;
    bool isPlaying() const;
    int  currentTrackId() const;
    void persistState();
    void restoreSession();
    void setLikedState(bool hasTrack, bool liked);

signals:
    void trackChanged(int trackId);
    void playingChanged(bool playing);
    void queueChanged();
    void queueRequested();
    void likeClicked();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void showTrackUi(const Track &track);
    void showEmptyUi();
    void syncTransportUi();
    void updateVolIcon();
    void applyResponsiveLayout(int width);
    QString buttonStyle(bool active = false) const;
    QString sliderStyle(const QString &accentColor) const;

    PlaybackEngine *m_engine = nullptr;

    VinylWidget *m_vinyl = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_artistLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_durationLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;

    QPushButton *m_playBtn = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_shuffleBtn = nullptr;
    QPushButton *m_repeatBtn = nullptr;
    QPushButton *m_queueBtn = nullptr;
    QPushButton *m_likeBtn  = nullptr;
    bool m_likeHasTrack = false;
    bool m_liked = false;

    ClickableSlider *m_progressSlider = nullptr;
    ClickableSlider *m_volumeSlider = nullptr;
    QPushButton     *m_volIcon = nullptr;

    QWidget *m_controlsContainer = nullptr;
    QWidget *m_leftWidget = nullptr;
    QWidget *m_rightWidget = nullptr;
};

#endif // PLAYERBAR_H
