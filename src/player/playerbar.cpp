#include "design/stylesheet.h"
#include "playerbar.h"
#include "lang.h"
#include "database.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QSettings>

PlayerBar::PlayerBar(TrackModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    setFixedHeight(90);
    lumen::design::StyleSheet::apply(this, QString("PlayerBar { background-color: %1; border-top: 1px solid %2; }")
        .arg(Theme::surface().name(), Theme::border().name()));

    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);

    // Restore the volume and mute state from the last session (theme and
    // downloadDir live in QSettings too, so playback audio joins them).
    const int savedVolume = qBound(0, QSettings().value("volume", 70).toInt(), 100);
    m_audioOutput->setVolume(savedVolume / 100.0);
    m_audioOutput->setMuted(QSettings().value("muted", false).toBool());

    // ── Layout ──────────────────────────────────────────────
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(16, 0, 16, 0);
    mainLayout->setSpacing(12);

    // Left: vinyl + track info
    auto *leftLayout = new QHBoxLayout();
    leftLayout->setSpacing(12);

    m_vinyl = new VinylWidget(52, this);
    leftLayout->addWidget(m_vinyl);

    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(1);
    m_titleLabel = new QLabel("", this);
    m_titleLabel->setFont(Theme::bodyFont(13));
    lumen::design::StyleSheet::apply(m_titleLabel, QString("color: %1; font-weight: 600; background: transparent;").arg(Theme::text().name()));
    m_titleLabel->setMaximumWidth(180);

    m_artistLabel = new QLabel("", this);
    m_artistLabel->setFont(Theme::bodyFont(11));
    lumen::design::StyleSheet::apply(m_artistLabel, QString("color: %1; background: transparent;").arg(Theme::textSoft().name()));
    m_artistLabel->setMaximumWidth(180);

    infoLayout->addStretch();
    infoLayout->addWidget(m_titleLabel);
    infoLayout->addWidget(m_artistLabel);
    infoLayout->addStretch();

    leftLayout->addLayout(infoLayout);
    leftLayout->addStretch();

    auto *leftWidget = new QWidget(this);
    leftWidget->setLayout(leftLayout);
    leftWidget->setFixedWidth(240);
    lumen::design::StyleSheet::apply(leftWidget, "background: transparent;");
    mainLayout->addWidget(leftWidget);

    // Center: controls + progress
    m_controlsContainer = new QWidget(this);
    lumen::design::StyleSheet::apply(m_controlsContainer, "background: transparent;");
    auto *centerLayout = new QVBoxLayout(m_controlsContainer);
    centerLayout->setContentsMargins(0, 8, 0, 8);
    centerLayout->setSpacing(4);

    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(14);
    btnLayout->setAlignment(Qt::AlignCenter);

    m_shuffleBtn = new QPushButton("\uE14B", this);
    m_prevBtn    = new QPushButton("\uE100", this);
    m_playBtn    = new QPushButton("\uE102", this);
    m_nextBtn    = new QPushButton("\uE101", this);
    m_repeatBtn  = new QPushButton("\uE1CD", this);

    for (auto *btn : {m_shuffleBtn, m_prevBtn, m_nextBtn, m_repeatBtn}) {
        btn->setFixedSize(32, 32);
        btn->setCursor(Qt::PointingHandCursor);
        lumen::design::StyleSheet::apply(btn, buttonStyle(false));
    }

    m_playBtn->setFixedSize(38, 38);
    m_playBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(m_playBtn, QString(
        "QPushButton { background-color: %1; color: %2; border: none; border-radius: 19px; font-size: 16px; font-family: \"Segoe MDL2 Assets\"; }"
        "QPushButton:hover { background-color: %3; }"
    ).arg(Theme::accent().name(), Theme::bg().name(), Theme::accent().lighter(110).name()));

    btnLayout->addWidget(m_shuffleBtn);
    btnLayout->addWidget(m_prevBtn);
    btnLayout->addWidget(m_playBtn);
    btnLayout->addWidget(m_nextBtn);
    btnLayout->addWidget(m_repeatBtn);

    centerLayout->addLayout(btnLayout);

    // Progress row
    auto *progressLayout = new QHBoxLayout();
    progressLayout->setSpacing(8);

    m_timeLabel = new QLabel("0:00", this);
    m_timeLabel->setFont(Theme::monoFont(10));
    lumen::design::StyleSheet::apply(m_timeLabel, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    m_timeLabel->setFixedWidth(36);
    m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_progressSlider = new ClickableSlider(Qt::Horizontal, this);
    m_progressSlider->setRange(0, 1000);
    m_progressSlider->setValue(0);
    lumen::design::StyleSheet::apply(m_progressSlider, sliderStyle(Theme::accent().name()));

    m_durationLabel = new QLabel("0:00", this);
    m_durationLabel->setFont(Theme::monoFont(10));
    lumen::design::StyleSheet::apply(m_durationLabel, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    m_durationLabel->setFixedWidth(36);
    m_durationLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    progressLayout->addWidget(m_timeLabel);
    progressLayout->addWidget(m_progressSlider);
    progressLayout->addWidget(m_durationLabel);

    centerLayout->addLayout(progressLayout);

    mainLayout->addWidget(m_controlsContainer, 1);

    // Right: volume
    auto *volLayout = new QHBoxLayout();
    volLayout->setContentsMargins(0, 0, 0, 0);
    volLayout->setSpacing(8);

    m_queueBtn = new QPushButton("\uE8FD", this);   // list/queue glyph
    m_queueBtn->setFixedSize(24, 24);
    m_queueBtn->setCursor(Qt::PointingHandCursor);
    m_queueBtn->setFont(Theme::iconFont(14));
    m_queueBtn->setToolTip(Lang::tr("Fila de reprodu\u00E7\u00E3o"));
    lumen::design::StyleSheet::apply(m_queueBtn, QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 4px; }"
        "QPushButton:hover { color: %2; background: rgba(255,255,255,0.05); }"
    ).arg(Theme::textMuted().name(), Theme::textSoft().name()));
    connect(m_queueBtn, &QPushButton::clicked, this, &PlayerBar::queueRequested);

    m_volIcon = new QPushButton("\uE15D", this);
    m_volIcon->setFixedSize(24, 24);
    m_volIcon->setCursor(Qt::PointingHandCursor);
    m_volIcon->setFont(Theme::iconFont(14));
    m_volIcon->setToolTip(Lang::tr("Silenciar"));
    lumen::design::StyleSheet::apply(m_volIcon, QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 4px; }"
        "QPushButton:hover { color: %2; background: rgba(255,255,255,0.05); }"
    ).arg(Theme::textMuted().name(), Theme::textSoft().name()));

    m_volumeSlider = new ClickableSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(savedVolume);
    m_volumeSlider->setFixedSize(100, 24);
    lumen::design::StyleSheet::apply(m_volumeSlider, sliderStyle(Theme::textSoft().name()));
    updateVolIcon();   // reflect the restored mute state on the icon

    // Everything pinned to the same 24px center line, with uniform spacing.
    volLayout->addStretch();
    volLayout->addWidget(m_queueBtn, 0, Qt::AlignVCenter);
    volLayout->addWidget(m_volIcon, 0, Qt::AlignVCenter);
    volLayout->addWidget(m_volumeSlider, 0, Qt::AlignVCenter);

    auto *rightWidget = new QWidget(this);
    rightWidget->setLayout(volLayout);
    rightWidget->setFixedWidth(196);
    lumen::design::StyleSheet::apply(rightWidget, "background: transparent;");
    mainLayout->addWidget(rightWidget);

    // Empty state label
    m_emptyLabel = new QLabel(Lang::tr("Adicione músicas para começar a ouvir"), this);
    m_emptyLabel->setFont(Theme::bodyFont(12));
    lumen::design::StyleSheet::apply(m_emptyLabel, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    m_emptyLabel->setAlignment(Qt::AlignCenter);

    m_controlsContainer->hide();
    leftWidget->hide();
    rightWidget->hide();

    // ── Connections ─────────────────────────────────────────
    connect(m_playBtn, &QPushButton::clicked, this, &PlayerBar::togglePlay);
    connect(m_prevBtn, &QPushButton::clicked, this, &PlayerBar::prev);
    connect(m_nextBtn, &QPushButton::clicked, this, &PlayerBar::next);

    connect(m_shuffleBtn, &QPushButton::clicked, this, [this]() {
        m_shuffle = !m_shuffle;
        lumen::design::StyleSheet::apply(m_shuffleBtn, buttonStyle(m_shuffle));
    });

    connect(m_repeatBtn, &QPushButton::clicked, this, [this]() {
        m_repeat = !m_repeat;
        lumen::design::StyleSheet::apply(m_repeatBtn, buttonStyle(m_repeat));
    });

    connect(m_progressSlider, &QSlider::sliderMoved, this, [this](int val) {
        qint64 dur = m_player->duration();
        if (dur > 0) {
            m_player->setPosition(dur * val / 1000);
        }
    });

    connect(m_volIcon, &QPushButton::clicked, this, [this]() {
        m_audioOutput->setMuted(!m_audioOutput->isMuted());
        QSettings().setValue("muted", m_audioOutput->isMuted());
        updateVolIcon();
    });

    connect(m_volumeSlider, &QSlider::sliderMoved, this, [this](int val) {
        m_audioOutput->setVolume(val / 100.0);
        QSettings().setValue("volume", val);
        if (m_audioOutput->isMuted()) {
            m_audioOutput->setMuted(false);
            QSettings().setValue("muted", false);
            updateVolIcon();
        }
    });

    connect(m_player, &QMediaPlayer::positionChanged, this, &PlayerBar::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &PlayerBar::onDurationChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &PlayerBar::onMediaStatusChanged);
}

void PlayerBar::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // The empty-state label isn't in a layout (the controls take over once a
    // track loads), so keep it spanning the whole bar — that way the centered
    // text never gets clipped on a narrow window.
    if (m_emptyLabel) m_emptyLabel->setGeometry(rect());
}

void PlayerBar::playTrack(const Track &track, const QList<Track> &queue) {
    m_queue = queue;   // set the playback context for next/prev/auto-advance
    if (m_currentTrackId == track.id) {
        togglePlay();
        return;
    }
    loadAndPlay(track);
}

void PlayerBar::playKeepingContext(const Track &track) {
    if (m_currentTrackId == track.id) { togglePlay(); return; }
    loadAndPlay(track);
}

const QList<Track> &PlayerBar::activeQueue() const {
    // Fall back to the full library when no explicit queue was given.
    return m_queue.isEmpty() ? m_model->tracks() : m_queue;
}

void PlayerBar::enqueue(const Track &track) {
    // If nothing is playing yet, just start it instead of only queueing.
    if (m_currentTrackId == 0) {
        loadAndPlay(track);
        return;
    }
    m_userQueue.append(track);
    emit queueChanged();
}

void PlayerBar::removeFromQueue(int index) {
    if (index < 0 || index >= m_userQueue.size()) return;
    m_userQueue.removeAt(index);
    emit queueChanged();
}

bool PlayerBar::takeFromQueue(int index, Track &out) {
    if (index < 0 || index >= m_userQueue.size()) return false;
    out = m_userQueue.takeAt(index);
    emit queueChanged();
    return true;
}

QList<Track> PlayerBar::upcomingContext() const {
    const QList<Track> &q = m_queue.isEmpty() ? m_model->tracks() : m_queue;
    int idx = -1;
    for (int i = 0; i < q.size(); ++i)
        if (q[i].id == m_currentTrackId) { idx = i; break; }
    if (idx < 0) return {};
    return q.mid(idx + 1);
}

void PlayerBar::showTrackUi(const Track &track) {
    m_titleLabel->setText(track.title);
    m_artistLabel->setText(track.artist);
    m_vinyl->setGradient(track.cover);

    m_emptyLabel->hide();
    m_controlsContainer->show();
    // Show all child widgets (reveals left/right panels hidden in constructor)
    for (auto *child : findChildren<QWidget *>()) {
        child->show();
    }
    m_emptyLabel->hide();
}

void PlayerBar::loadAndPlay(const Track &track) {
    m_currentTrackId = track.id;
    m_currentTrack   = track;
    m_pendingSeekMs  = 0;   // a fresh track starts from the beginning
    m_player->setSource(track.audioUrl);
    m_player->play();

    showTrackUi(track);
    m_vinyl->setSpinning(true);
    m_playBtn->setText("\uE103");

    updateControls();
    emit trackChanged(m_currentTrackId);
    emit playingChanged(true);

    // Record this play in the listening history (last_played_at + play_count).
    m_model->markPlayed(track.id);
}

void PlayerBar::persistState() {
    Database::PlaybackState s;
    s.trackId = m_currentTrackId;
    s.posMs   = m_currentTrackId != 0 ? m_player->position() : 0;
    s.volume  = qBound(0.0, double(m_audioOutput->volume()), 1.0);
    s.shuffle = m_shuffle;
    s.repeat  = m_repeat;
    Database::instance().saveState(s);
}

void PlayerBar::restoreSession() {
    const auto s = Database::instance().loadState();
    if (s.trackId == 0) return;
    Track *t = m_model->findTrack(s.trackId);
    if (!t) return;   // track was deleted since the last session

    m_shuffle = s.shuffle;
    m_repeat  = s.repeat;

    m_currentTrackId = t->id;
    m_currentTrack   = *t;
    m_player->setSource(t->audioUrl);
    m_player->pause();              // load the media without playing
    m_pendingSeekMs = s.posMs;      // applied once the media finishes loading

    showTrackUi(*t);
    m_vinyl->setSpinning(false);
    m_playBtn->setText("\uE102");   // play glyph: resumes where it stopped

    updateControls();
    emit trackChanged(m_currentTrackId);
    emit playingChanged(false);
}

void PlayerBar::togglePlay() {
    if (m_currentTrackId == 0) return;

    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
        m_playBtn->setText("\uE102");
        m_vinyl->setSpinning(false);
        emit playingChanged(false);
    } else {
        m_player->play();
        m_playBtn->setText("\uE103");
        m_vinyl->setSpinning(true);
        emit playingChanged(true);
    }
}

void PlayerBar::next() {
    if (m_currentTrackId == 0) return;

    // Repeat-one only affects the automatic advance at the end of a track
    // (handled in onMediaStatusChanged); pressing "next" always skips ahead.

    // Manually queued tracks play before continuing the context.
    if (!m_userQueue.isEmpty()) {
        Track t = m_userQueue.takeFirst();
        emit queueChanged();
        loadAndPlay(t);
        return;
    }

    const QList<Track> &queue = activeQueue();
    if (queue.isEmpty()) return;

    int idx = -1;
    for (int i = 0; i < queue.size(); ++i) {
        if (queue[i].id == m_currentTrackId) { idx = i; break; }
    }
    // Current track isn't part of the context (e.g. it came from the queue):
    // start the context from its beginning.
    if (idx < 0) { loadAndPlay(queue.first()); return; }

    int nextIdx = m_shuffle ? QRandomGenerator::global()->bounded(queue.size())
                            : (idx + 1) % queue.size();
    loadAndPlay(queue[nextIdx]);
}

void PlayerBar::prev() {
    if (m_currentTrackId == 0) return;
    if (m_player->position() > 3000) {
        m_player->setPosition(0);
        return;
    }
    const QList<Track> &queue = activeQueue();
    if (queue.isEmpty()) return;

    int idx = -1;
    for (int i = 0; i < queue.size(); ++i) {
        if (queue[i].id == m_currentTrackId) { idx = i; break; }
    }
    if (idx < 0) return;

    int prevIdx = (idx - 1 + queue.size()) % queue.size();
    loadAndPlay(queue[prevIdx]);
}

void PlayerBar::setShuffle(bool on) { m_shuffle = on; }
void PlayerBar::setRepeat(bool on) { m_repeat = on; }
bool PlayerBar::isPlaying() const { return m_player->playbackState() == QMediaPlayer::PlayingState; }
int  PlayerBar::currentTrackId() const { return m_currentTrackId; }

void PlayerBar::onPositionChanged(qint64 pos) {
    m_timeLabel->setText(Theme::formatTime(pos));
    qint64 dur = m_player->duration();
    if (dur > 0 && !m_progressSlider->isSliderDown()) {
        m_progressSlider->setValue(static_cast<int>(pos * 1000 / dur));
    }
}

void PlayerBar::onDurationChanged(qint64 dur) {
    m_durationLabel->setText(Theme::formatTime(dur));
    if (m_currentTrackId != 0) {
        m_model->setDuration(m_currentTrackId, dur);
    }
}

void PlayerBar::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    // Apply the position restored from the last session once the media is
    // actually seekable.
    if ((status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia)
            && m_pendingSeekMs > 0) {
        m_player->setPosition(m_pendingSeekMs);
        m_pendingSeekMs = 0;
    }

    if (status == QMediaPlayer::EndOfMedia) {
        if (m_repeat) {
            m_player->setPosition(0);
            m_player->play();
        } else {
            next();
        }
    }
}

void PlayerBar::updateControls() {
    lumen::design::StyleSheet::apply(m_shuffleBtn, buttonStyle(m_shuffle));
    lumen::design::StyleSheet::apply(m_repeatBtn, buttonStyle(m_repeat));
}

QString PlayerBar::buttonStyle(bool active) const {
    QString color = active ? Theme::accent().name() : Theme::textMuted().name();
    QString hoverColor = active ? Theme::accent().lighter(110).name() : Theme::textSoft().name();
    return QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 16px; font-size: 14px; font-family: \"Segoe MDL2 Assets\"; }"
        "QPushButton:hover { color: %2; background: rgba(255,255,255,0.05); }"
    ).arg(color, hoverColor);
}

void PlayerBar::updateVolIcon() {
    bool muted = m_audioOutput->isMuted();
    m_volIcon->setText(muted ? "\uE198" : "\uE15D");
    m_volIcon->setToolTip(muted ? Lang::tr("Ativar som") : Lang::tr("Silenciar"));
}

QString PlayerBar::sliderStyle(const QString &accentColor) const {
    return QString(R"(
        QSlider::groove:horizontal {
            height: 4px;
            background: rgba(255,255,255,0.08);
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: %2;
            width: 12px; height: 12px;
            margin: -4px 0;
            border-radius: 6px;
        }
        QSlider::handle:horizontal:hover {
            background: #f0ece4;
        }
        QSlider::sub-page:horizontal {
            background: %1;
            border-radius: 2px;
        }
    )").arg(accentColor, Theme::text().name());
}