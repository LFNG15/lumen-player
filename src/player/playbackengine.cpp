#include "playbackengine.h"
#include "database.h"

#include <QMediaPlayer>
#include <QAudioOutput>
#include <QRandomGenerator>
#include <QTimer>
#include <QDebug>
#include <algorithm>

PlaybackEngine::PlaybackEngine(TrackModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
{
    m_player = new QMediaPlayer(this);
    m_audio  = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);

    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 ms) {
        m_stateDirty = true;
        emit positionChanged(ms);
    });
    connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 dur) {
        if (m_currentTrackId != 0 && m_model)
            m_model->setDuration(m_currentTrackId, dur);
        emit durationChanged(dur);
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, [this](QMediaPlayer::MediaStatus st) {
        onMediaStatusChanged(static_cast<int>(st));
    });
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState st) {
        onPlaybackStateChanged(static_cast<int>(st));
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &str) {
        qWarning() << "QMediaPlayer error" << str << m_currentTrack.audioUrl;
        reportInvalidMedia();
    });

    // Windows logoff/kill skip closeEvent; persist dirty state every ~30s.
    m_persistTimer = new QTimer(this);
    m_persistTimer->setInterval(30'000);
    connect(m_persistTimer, &QTimer::timeout, this, [this]() {
        if (m_stateDirty)
            persistState();
    });
    m_persistTimer->start();
}

// --- Transport ----------------------------------------------------------------

void PlaybackEngine::playTrack(const Track &track, const QList<Track> &queue,
                               const QString &contextName)
{
    m_context = queue;
    m_contextName = contextName;
    m_contextIndex = -1; // recomputed by loadAndPlay() once the track is loaded
    markStateDirty();
    if (m_shuffle)
        rebuildShuffleBag();

    if (m_currentTrackId == track.id) {
        togglePlay();
        return;
    }
    loadAndPlay(track);
}

void PlaybackEngine::playKeepingContext(const Track &track)
{
    if (m_currentTrackId == track.id) {
        togglePlay();
        return;
    }
    loadAndPlay(track);
}

void PlaybackEngine::loadAndPlay(const Track &track, bool markPlayed)
{
    m_restorePending = false;
    m_errorNotified  = false;
    m_currentTrackId = track.id;
    m_currentTrack   = track;
    m_pendingSeekMs  = 0;

    // Track our place in the context so a detour through a manually-queued
    // track (not part of m_context) doesn't lose it — findInContext() would
    // return -1 for that track and next()/upcomingContext() would otherwise
    // think the context is exhausted instead of just paused.
    const int idx = findInContext(track.id);
    if (idx >= 0) m_contextIndex = idx;
    m_player->setSource(track.audioUrl);
    m_player->play();

    emit trackChanged(m_currentTrackId);
    emit playingChanged(true);

    if (markPlayed && m_model)
        m_model->markPlayed(track.id);
    markStateDirty();
}

void PlaybackEngine::togglePlay()
{
    if (m_currentTrackId == 0) return;

    if (m_restorePending) {
        startPendingRestore();
        return;
    }

    if (m_player->mediaStatus() == QMediaPlayer::InvalidMedia
        || m_player->error() != QMediaPlayer::NoError) {
        reportInvalidMedia();
        return;
    }

    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
        emit playingChanged(false);
    } else {
        m_player->play();
        applyPendingSeekIfReady();
        emit playingChanged(true);
    }
    markStateDirty();
}

void PlaybackEngine::stop()
{
    m_player->stop();
    emit playingChanged(false);
}

void PlaybackEngine::seek(qint64 ms)
{
    if (m_currentTrackId == 0) return;
    if (m_restorePending) {
        m_pendingSeekMs = ms;
        startPendingRestore();
        return;
    }
    m_player->setPosition(ms);
    markStateDirty();
}

const QList<Track> &PlaybackEngine::activeContext() const
{
    return m_context.isEmpty() && m_model ? m_model->tracks() : m_context;
}

QList<Track> PlaybackEngine::contextQueue() const
{
    return activeContext();
}

int PlaybackEngine::findInContext(int trackId) const
{
    const QList<Track> &q = activeContext();
    for (int i = 0; i < q.size(); ++i)
        if (q[i].id == trackId) return i;
    return -1;
}

void PlaybackEngine::rebuildShuffleBag()
{
    const QList<Track> &q = activeContext();
    m_shuffleBag.resize(q.size());
    for (int i = 0; i < q.size(); ++i)
        m_shuffleBag[i] = i;

    // Fisher–Yates
    auto *rng = QRandomGenerator::global();
    for (int i = m_shuffleBag.size() - 1; i > 0; --i) {
        const int j = rng->bounded(i + 1);
        std::swap(m_shuffleBag[i], m_shuffleBag[j]);
    }
    m_shufflePos = 0;

    // Start after the current track if it's in the bag, so next() doesn't
    // immediately re-pick it.
    const int cur = findInContext(m_currentTrackId);
    if (cur >= 0) {
        const int at = m_shuffleBag.indexOf(cur);
        if (at >= 0) {
            std::swap(m_shuffleBag[0], m_shuffleBag[at]);
            m_shufflePos = 1 % qMax(1, m_shuffleBag.size());
        }
    }
}

int PlaybackEngine::nextShuffleIndex()
{
    const QList<Track> &q = activeContext();
    if (q.isEmpty()) return -1;
    if (m_shuffleBag.isEmpty() || m_shuffleBag.size() != q.size()
        || m_shufflePos >= m_shuffleBag.size()) {
        rebuildShuffleBag();
    }
    if (m_shuffleBag.isEmpty()) return -1;
    const int idx = m_shuffleBag[m_shufflePos];
    m_shufflePos = (m_shufflePos + 1) % m_shuffleBag.size();
    // When we wrap, reshuffle for the next cycle.
    if (m_shufflePos == 0)
        rebuildShuffleBag();
    return idx;
}

void PlaybackEngine::next()
{
    if (m_currentTrackId == 0) return;

    // Manual queue first.
    if (!m_userQueue.isEmpty()) {
        Track t = m_userQueue.takeFirst();
        emit queueChanged();
        loadAndPlay(t);
        return;
    }

    const QList<Track> &queue = activeContext();
    if (queue.isEmpty()) return;

    int idx = findInContext(m_currentTrackId);
    if (idx < 0) idx = m_contextIndex; // currently on a manual-queue track — resume from there
    if (idx < 0) {
        loadAndPlay(queue.first());
        return;
    }

    int nextIdx = -1;
    if (m_shuffle) {
        nextIdx = nextShuffleIndex();
    } else {
        nextIdx = idx + 1;
        if (nextIdx >= queue.size()) {
            if (m_repeat == RepeatMode::All)
                nextIdx = 0;
            else
                return; // end of context, stop advancing
        }
    }
    if (nextIdx >= 0 && nextIdx < queue.size())
        loadAndPlay(queue[nextIdx]);
}

void PlaybackEngine::prev()
{
    if (m_currentTrackId == 0) return;
    if (m_player->position() > 3000) {
        m_player->setPosition(0);
        return;
    }

    const QList<Track> &queue = activeContext();
    if (queue.isEmpty()) return;

    int idx = findInContext(m_currentTrackId);
    if (idx < 0) idx = m_contextIndex; // currently on a manual-queue track — resume from there
    if (idx < 0) return;

    int prevIdx;
    if (m_shuffle) {
        // Walk the bag backwards when possible; otherwise previous linear.
        prevIdx = (idx - 1 + queue.size()) % queue.size();
    } else {
        prevIdx = (idx - 1 + queue.size()) % queue.size();
    }
    loadAndPlay(queue[prevIdx]);
}

// --- Modes --------------------------------------------------------------------

void PlaybackEngine::setShuffle(bool on)
{
    if (m_shuffle == on) return;
    m_shuffle = on;
    if (m_shuffle)
        rebuildShuffleBag();
    else {
        m_shuffleBag.clear();
        m_shufflePos = 0;
    }
    emit shuffleChanged(m_shuffle);
    markStateDirty();
}

void PlaybackEngine::setRepeatMode(RepeatMode mode)
{
    if (m_repeat == mode) return;
    m_repeat = mode;
    emit repeatModeChanged(m_repeat);
    markStateDirty();
}

void PlaybackEngine::cycleRepeatMode()
{
    switch (m_repeat) {
    case RepeatMode::Off: setRepeatMode(RepeatMode::All); break;
    case RepeatMode::All: setRepeatMode(RepeatMode::One); break;
    case RepeatMode::One: setRepeatMode(RepeatMode::Off); break;
    }
}

// --- Volume -------------------------------------------------------------------

void PlaybackEngine::setVolume(double volume01)
{
    volume01 = qBound(0.0, volume01, 1.0);
    m_audio->setVolume(volume01);
    emit volumeChanged(volume01);
    markStateDirty();
}

double PlaybackEngine::volume() const
{
    return m_audio->volume();
}

void PlaybackEngine::setMuted(bool muted)
{
    if (m_audio->isMuted() == muted) return;
    m_audio->setMuted(muted);
    emit mutedChanged(muted);
    markStateDirty();
}

bool PlaybackEngine::isMuted() const
{
    return m_audio->isMuted();
}

// --- Queues -------------------------------------------------------------------

void PlaybackEngine::enqueue(const Track &track)
{
    if (m_currentTrackId == 0) {
        loadAndPlay(track);
        return;
    }
    m_userQueue.append(track);
    emit queueChanged();
    markStateDirty();
}

void PlaybackEngine::removeFromQueue(int index)
{
    if (index < 0 || index >= m_userQueue.size()) return;
    m_userQueue.removeAt(index);
    emit queueChanged();
    markStateDirty();
}

bool PlaybackEngine::takeFromQueue(int index, Track &out)
{
    if (index < 0 || index >= m_userQueue.size()) return false;
    out = m_userQueue.takeAt(index);
    emit queueChanged();
    markStateDirty();
    return true;
}

void PlaybackEngine::clearUserQueue()
{
    if (m_userQueue.isEmpty()) return;
    m_userQueue.clear();
    emit queueChanged();
    markStateDirty();
}

void PlaybackEngine::reorderUserQueue(int from, int to)
{
    if (from < 0 || from >= m_userQueue.size() || to < 0 || to >= m_userQueue.size() || from == to)
        return;
    m_userQueue.move(from, to);
    emit queueChanged();
    markStateDirty();
}

QList<Track> PlaybackEngine::upcomingContext() const
{
    const QList<Track> &q = activeContext();
    int idx = findInContext(m_currentTrackId);
    if (idx < 0) idx = m_contextIndex; // currently on a manual-queue track — show what's paused
    if (idx < 0) return {};
    return q.mid(idx + 1);
}

bool PlaybackEngine::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

qint64 PlaybackEngine::position() const
{
    if (m_restorePending && m_pendingSeekMs > 0)
        return m_pendingSeekMs;
    return m_player->position();
}
qint64 PlaybackEngine::duration() const
{
    if (m_restorePending && m_currentTrack.durationMs > 0)
        return m_currentTrack.durationMs;
    return m_player->duration();
}

// --- Media callbacks ----------------------------------------------------------

void PlaybackEngine::onMediaStatusChanged(int status)
{
    const auto st = static_cast<QMediaPlayer::MediaStatus>(status);

    applyPendingSeekIfReady();

    if (st == QMediaPlayer::InvalidMedia)
        reportInvalidMedia();

    if (st == QMediaPlayer::EndOfMedia) {
        if (m_repeat == RepeatMode::One) {
            m_player->setPosition(0);
            m_player->play();
        } else {
            // All / Off: next() respects All wrap; Off stops at end.
            next();
        }
    }
}

void PlaybackEngine::onPlaybackStateChanged(int state)
{
    if (state == static_cast<int>(QMediaPlayer::PlayingState))
        applyPendingSeekIfReady();
}

void PlaybackEngine::applyPendingSeekIfReady()
{
    if (m_pendingSeekMs <= 0) return;
    const auto st = m_player->mediaStatus();
    if (st != QMediaPlayer::LoadedMedia && st != QMediaPlayer::BufferedMedia
        && st != QMediaPlayer::BufferingMedia)
        return;
    m_player->setPosition(m_pendingSeekMs);
    m_pendingSeekMs = 0;
}

void PlaybackEngine::startPendingRestore()
{
    if (!m_restorePending) return;
    m_restorePending = false;
    m_errorNotified  = false;
    if (m_currentTrack.audioUrl.isEmpty()) {
        reportInvalidMedia();
        return;
    }
    m_player->setSource(m_currentTrack.audioUrl);
    m_player->play();
    applyPendingSeekIfReady();
    emit playingChanged(true);
    markStateDirty();
}

void PlaybackEngine::reportInvalidMedia()
{
    emit playingChanged(false);
    if (m_errorNotified) return;
    m_errorNotified = true;
    emit playbackError(QStringLiteral("Não foi possível reproduzir esta faixa"));
}

// --- Persistence --------------------------------------------------------------

void PlaybackEngine::markStateDirty()
{
    m_stateDirty = true;
}

void PlaybackEngine::persistState()
{
    Database::PlaybackState s;
    s.trackId    = m_currentTrackId;
    s.posMs      = m_currentTrackId == 0 ? 0
                 : (m_restorePending ? m_pendingSeekMs : m_player->position());
    s.volume     = qBound(0.0, m_audio->volume(), 1.0);
    s.muted      = m_audio->isMuted();
    s.shuffle    = m_shuffle;
    s.repeatMode = static_cast<int>(m_repeat);

    // Product: if the user never clicked a track this session, m_context is
    // empty but playback still walks activeContext() (library fallback).
    // Persist that effective queue so the fila survives restart.
    s.contextIds.clear();
    const QList<Track> &ctx =
        (m_context.isEmpty() && m_currentTrackId != 0) ? activeContext() : m_context;
    for (const auto &t : ctx)
        s.contextIds.append(t.id);

    s.userQueueIds.clear();
    for (const auto &t : m_userQueue)
        s.userQueueIds.append(t.id);

    s.contextIndex = m_contextIndex;
    s.contextName  = m_contextName;

    qDebug() << "persistState track" << s.trackId << "pos" << s.posMs
             << "vol" << s.volume << "ctx" << s.contextIds.size();
    Database::instance().saveState(s);
    m_stateDirty = false;
}

void PlaybackEngine::restoreSession()
{
    const auto s = Database::instance().loadState();

    // Volume: DB is the single source of truth (Task P4).
    setVolume(s.volume);
    setMuted(s.muted);

    m_shuffle = s.shuffle;
    m_repeat  = static_cast<RepeatMode>(qBound(0, s.repeatMode, 2));
    emit shuffleChanged(m_shuffle);
    emit repeatModeChanged(m_repeat);

    // Rebuild queues from persisted ids.
    m_context.clear();
    for (int id : s.contextIds) {
        if (Track *t = m_model->findTrack(id))
            m_context.append(*t);
    }
    m_userQueue.clear();
    for (int id : s.userQueueIds) {
        if (Track *t = m_model->findTrack(id))
            m_userQueue.append(*t);
    }
    if (!m_userQueue.isEmpty() || !m_context.isEmpty())
        emit queueChanged();

    if (s.trackId == 0) {
        m_stateDirty = false;
        qDebug() << "restoreSession empty vol" << s.volume;
        return;
    }
    Track *t = m_model->findTrack(s.trackId);
    if (!t) {
        m_stateDirty = false;
        qDebug() << "restoreSession missing track" << s.trackId;
        return;
    }

    m_currentTrackId = t->id;
    m_currentTrack   = *t;
    m_contextName    = s.contextName;
    // Prefer the persisted position (valid across a manual-queue detour); fall
    // back to a fresh lookup if the context shrank (e.g. tracks removed) and
    // the saved index no longer lines up.
    m_contextIndex = (s.contextIndex >= 0 && s.contextIndex < m_context.size())
        ? s.contextIndex : findInContext(m_currentTrackId);
    // Lazy-load: setSource+pause() on restore leaves the FFmpeg backend stuck
    // so the next play() is silent until the user seeks. Keep source+position
    // and only load on the first play().
    m_pendingSeekMs = s.posMs;
    m_restorePending = true;

    if (m_shuffle)
        rebuildShuffleBag();

    emit trackChanged(m_currentTrackId);
    emit playingChanged(false);
    if (m_pendingSeekMs > 0)
        emit positionChanged(m_pendingSeekMs);
    if (m_currentTrack.durationMs > 0)
        emit durationChanged(m_currentTrack.durationMs);
    m_stateDirty = false;
    qDebug() << "restoreSession track" << m_currentTrackId << "pos" << m_pendingSeekMs
             << "vol" << s.volume << "ctx" << m_context.size() << "lazy";
}
