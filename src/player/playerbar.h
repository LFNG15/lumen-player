#ifndef PLAYERBAR_H
#define PLAYERBAR_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QMediaPlayer>
#include <QAudioOutput>
#include "clickableslider.h"
#include "vinylwidget.h"
#include "trackmodel.h"

class PlayerBar : public QWidget {
    Q_OBJECT
public:
    explicit PlayerBar(TrackModel *model, QWidget *parent = nullptr);

    // Plays a track within a given queue (playlist, liked list, library…).
    // next/prev and auto-advance stay within that queue. An empty queue falls
    // back to the full library.
    void playTrack(const Track &track, const QList<Track> &queue = QList<Track>());
    void playKeepingContext(const Track &track);  // play without resetting context/queue
    void togglePlay();
    void next();
    void prev();
    void setShuffle(bool on);
    void setRepeat(bool on);

    // Spotify-style manual queue ("Próximas na fila").
    void enqueue(const Track &track);
    void removeFromQueue(int index);
    bool takeFromQueue(int index, Track &out);   // removes and returns the item
    QList<Track> userQueue() const { return m_userQueue; }
    QList<Track> upcomingContext() const;        // context tracks after the current one
    Track currentTrack() const { return m_currentTrack; }

    bool isPlaying() const;
    int  currentTrackId() const;

    // Session persistence: the last track and position are stored in the
    // database on quit and restored (paused) on the next launch.
    void persistState();
    void restoreSession();

signals:
    void trackChanged(int trackId);
    void playingChanged(bool playing);
    void queueChanged();
    void queueRequested();   // user asked to open the queue view

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onPositionChanged(qint64 pos);
    void onDurationChanged(qint64 dur);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);

private:
    void loadAndPlay(const Track &track);
    void showTrackUi(const Track &track);
    const QList<Track> &activeQueue() const;
    void updateControls();
    QString buttonStyle(bool active = false) const;
    QString sliderStyle(const QString &accentColor) const;

    TrackModel *m_model;
    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;

    VinylWidget *m_vinyl;
    QLabel *m_titleLabel;
    QLabel *m_artistLabel;
    QLabel *m_timeLabel;
    QLabel *m_durationLabel;
    QLabel *m_emptyLabel;

    QPushButton *m_playBtn;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_shuffleBtn;
    QPushButton *m_repeatBtn;
    QPushButton *m_queueBtn;

    ClickableSlider *m_progressSlider;
    ClickableSlider *m_volumeSlider;
    QPushButton     *m_volIcon;

    QWidget *m_controlsContainer;

    int  m_currentTrackId = 0;
    bool m_shuffle = false;
    bool m_repeat  = false;
    qint64 m_pendingSeekMs = 0;   // restored position, applied once media loads
    QList<Track> m_queue;       // current playback context; empty = full library
    QList<Track> m_userQueue;   // manually queued tracks, played before the context
    Track        m_currentTrack;

    void updateVolIcon();
};

#endif // PLAYERBAR_H