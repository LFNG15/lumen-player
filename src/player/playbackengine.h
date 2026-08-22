#ifndef PLAYBACKENGINE_H
#define PLAYBACKENGINE_H

#include <QObject>
#include <QList>
#include <QVector>
#include "trackmodel.h"

class QMediaPlayer;
class QAudioOutput;
class QTimer;

// Playback engine (P4): owns QMediaPlayer, context queue, manual queue,
// shuffle bag, and repeat modes. PlayerBar is a pure view over this object.
class PlaybackEngine : public QObject {
    Q_OBJECT
public:
    enum class RepeatMode { Off = 0, All = 1, One = 2 };
    Q_ENUM(RepeatMode)

    explicit PlaybackEngine(TrackModel *model, QObject *parent = nullptr);

    // --- Transport ---
    void playTrack(const Track &track, const QList<Track> &queue = QList<Track>(),
                    const QString &contextName = QString());
    void playKeepingContext(const Track &track);
    void togglePlay();
    void next();
    void prev();
    void seek(qint64 ms);
    void stop();

    // --- Mode ---
    void setShuffle(bool on);
    bool shuffle() const { return m_shuffle; }
    void setRepeatMode(RepeatMode mode);
    void cycleRepeatMode();   // Off → All → One → Off
    RepeatMode repeatMode() const { return m_repeat; }

    // --- Volume (single source of truth: DB via persist/restore) ---
    void setVolume(double volume01);   // 0..1
    double volume() const;
    void setMuted(bool muted);
    bool isMuted() const;

    // --- Queues ---
    void enqueue(const Track &track);
    void removeFromQueue(int index);
    bool takeFromQueue(int index, Track &out);
    void clearUserQueue();
    void reorderUserQueue(int from, int to);
    QList<Track> userQueue() const { return m_userQueue; }
    QList<Track> contextQueue() const;
    QList<Track> upcomingContext() const;
    QString contextName() const { return m_contextName; }

    Track currentTrack() const { return m_currentTrack; }
    int   currentTrackId() const { return m_currentTrackId; }
    bool  isPlaying() const;
    qint64 position() const;
    qint64 duration() const;

    QMediaPlayer *mediaPlayer() const { return m_player; }
    QAudioOutput *audioOutput() const { return m_audio; }

    void persistState();
    void restoreSession();

signals:
    void trackChanged(int trackId);
    void playingChanged(bool playing);
    void queueChanged();
    void positionChanged(qint64 ms);
    void durationChanged(qint64 ms);
    void shuffleChanged(bool on);
    void repeatModeChanged(RepeatMode mode);
    void volumeChanged(double volume01);
    void mutedChanged(bool muted);
    void playbackError(const QString &message);

private slots:
    void onMediaStatusChanged(int status); // QMediaPlayer::MediaStatus
    void onPlaybackStateChanged(int state);

private:
    void loadAndPlay(const Track &track, bool markPlayed = true);
    const QList<Track> &activeContext() const;
    void rebuildShuffleBag();
    int  nextShuffleIndex();
    int  findInContext(int trackId) const;
    void markStateDirty();
    void startPendingRestore();
    void applyPendingSeekIfReady();
    void reportInvalidMedia();

    TrackModel   *m_model  = nullptr;
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audio  = nullptr;

    int        m_currentTrackId = 0;
    Track      m_currentTrack;
    QList<Track> m_context;     // playback context; empty = full library
    QString      m_contextName; // display name of m_context (e.g. folder/"Curtidas")
    int          m_contextIndex = -1; // last known position in m_context, kept while
                                       // a manual-queue track (not part of m_context) plays
    QList<Track> m_userQueue;   // manual "up next"

    bool       m_shuffle = false;
    RepeatMode m_repeat  = RepeatMode::Off;
    qint64     m_pendingSeekMs = 0;

    // Shuffle bag: permutation of context indices; advances without repeat
    // until the bag is exhausted, then reshuffles (like a deck).
    QVector<int> m_shuffleBag;
    int          m_shufflePos = 0;

    QTimer *m_persistTimer = nullptr;
    bool    m_stateDirty   = false;
    bool    m_restorePending = false;
    bool    m_errorNotified  = false;
};

#endif // PLAYBACKENGINE_H
