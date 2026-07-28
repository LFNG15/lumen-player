#include "playerbar.h"
#include "lang.h"
#include "theme.h"
#include "design/stylesheet.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QResizeEvent>

PlayerBar::PlayerBar(PlaybackEngine *engine, QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
{
    setFixedHeight(90);
    setObjectName(QStringLiteral("lumenPlayerBar"));
    lumen::design::StyleSheet::apply(this, QString(
        "PlayerBar { background-color: %1; border-top: 1px solid %2; }"
    ).arg(Theme::surface().name(), Theme::border().name()));

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(16, 0, 16, 0);
    mainLayout->setSpacing(12);

    // Left: vinyl + info
    auto *leftLayout = new QHBoxLayout();
    leftLayout->setSpacing(12);
    m_vinyl = new VinylWidget(52, this);
    leftLayout->addWidget(m_vinyl);

    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(1);
    m_titleLabel = new QLabel(QString(), this);
    m_titleLabel->setFont(Theme::bodyFont(13));
    lumen::design::StyleSheet::apply(m_titleLabel, QString(
        "color: %1; font-weight: 600; background: transparent;").arg(Theme::text().name()));
    m_titleLabel->setMaximumWidth(180);

    m_artistLabel = new QLabel(QString(), this);
    m_artistLabel->setFont(Theme::bodyFont(11));
    lumen::design::StyleSheet::apply(m_artistLabel, QString(
        "color: %1; background: transparent;").arg(Theme::textSoft().name()));
    m_artistLabel->setMaximumWidth(180);

    infoLayout->addStretch();
    infoLayout->addWidget(m_titleLabel);
    infoLayout->addWidget(m_artistLabel);
    infoLayout->addStretch();
    leftLayout->addLayout(infoLayout);
    leftLayout->addStretch();

    m_leftWidget = new QWidget(this);
    m_leftWidget->setLayout(leftLayout);
    m_leftWidget->setFixedWidth(240);
    lumen::design::StyleSheet::apply(m_leftWidget, QStringLiteral("background: transparent;"));
    mainLayout->addWidget(m_leftWidget);

    // Center
    m_controlsContainer = new QWidget(this);
    lumen::design::StyleSheet::apply(m_controlsContainer, QStringLiteral("background: transparent;"));
    auto *centerLayout = new QVBoxLayout(m_controlsContainer);
    centerLayout->setContentsMargins(0, 8, 0, 8);
    centerLayout->setSpacing(4);

    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(14);
    btnLayout->setAlignment(Qt::AlignCenter);

    m_shuffleBtn = new QPushButton(QStringLiteral("\uE14B"), this);
    m_prevBtn    = new QPushButton(QStringLiteral("\uE100"), this);
    m_playBtn    = new QPushButton(QStringLiteral("\uE102"), this);
    m_nextBtn    = new QPushButton(QStringLiteral("\uE101"), this);
    m_repeatBtn  = new QPushButton(QStringLiteral("\uE1CD"), this);

    for (auto *btn : {m_shuffleBtn, m_prevBtn, m_nextBtn, m_repeatBtn}) {
        btn->setFixedSize(32, 32);
        btn->setCursor(Qt::PointingHandCursor);
        lumen::design::StyleSheet::apply(btn, buttonStyle(false));
    }
    m_playBtn->setFixedSize(38, 38);
    m_playBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(m_playBtn, QString(
        "QPushButton { background-color: %1; color: %2; border: none; border-radius: 19px; "
        "font-size: 16px; font-family: \"Segoe MDL2 Assets\"; }"
        "QPushButton:hover { background-color: %3; }"
    ).arg(Theme::accent().name(), Theme::bg().name(), Theme::accent().lighter(110).name()));

    btnLayout->addWidget(m_shuffleBtn);
    btnLayout->addWidget(m_prevBtn);
    btnLayout->addWidget(m_playBtn);
    btnLayout->addWidget(m_nextBtn);
    btnLayout->addWidget(m_repeatBtn);
    centerLayout->addLayout(btnLayout);

    auto *progressLayout = new QHBoxLayout();
    progressLayout->setSpacing(8);
    m_timeLabel = new QLabel(QStringLiteral("0:00"), this);
    m_timeLabel->setFont(Theme::monoFont(10));
    lumen::design::StyleSheet::apply(m_timeLabel, QString(
        "color: %1; background: transparent;").arg(Theme::textMuted().name()));
    m_timeLabel->setFixedWidth(36);
    m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_progressSlider = new ClickableSlider(Qt::Horizontal, this);
    m_progressSlider->setRange(0, 1000);
    m_progressSlider->setValue(0);
    lumen::design::StyleSheet::apply(m_progressSlider, sliderStyle(Theme::accent().name()));

    m_durationLabel = new QLabel(QStringLiteral("0:00"), this);
    m_durationLabel->setFont(Theme::monoFont(10));
    lumen::design::StyleSheet::apply(m_durationLabel, QString(
        "color: %1; background: transparent;").arg(Theme::textMuted().name()));
    m_durationLabel->setFixedWidth(36);
    m_durationLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    progressLayout->addWidget(m_timeLabel);
    progressLayout->addWidget(m_progressSlider);
    progressLayout->addWidget(m_durationLabel);
    centerLayout->addLayout(progressLayout);
    mainLayout->addWidget(m_controlsContainer, 1);

    // Right
    auto *volLayout = new QHBoxLayout();
    volLayout->setContentsMargins(0, 0, 0, 0);
    volLayout->setSpacing(8);

    m_queueBtn = new QPushButton(QStringLiteral("\uE8FD"), this);
    m_queueBtn->setFixedSize(24, 24);
    m_queueBtn->setCursor(Qt::PointingHandCursor);
    m_queueBtn->setFont(Theme::iconFont(14));
    m_queueBtn->setToolTip(Lang::tr("Fila de reprodução"));
    lumen::design::StyleSheet::apply(m_queueBtn, QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 4px; }"
        "QPushButton:hover { color: %2; background: rgba(255,255,255,0.05); }"
    ).arg(Theme::textMuted().name(), Theme::textSoft().name()));

    m_volIcon = new QPushButton(QStringLiteral("\uE15D"), this);
    m_volIcon->setFixedSize(24, 24);
    m_volIcon->setCursor(Qt::PointingHandCursor);
    m_volIcon->setFont(Theme::iconFont(14));
    lumen::design::StyleSheet::apply(m_volIcon, QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 4px; }"
        "QPushButton:hover { color: %2; background: rgba(255,255,255,0.05); }"
    ).arg(Theme::textMuted().name(), Theme::textSoft().name()));

    m_volumeSlider = new ClickableSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(70);
    m_volumeSlider->setFixedSize(100, 24);
    lumen::design::StyleSheet::apply(m_volumeSlider, sliderStyle(Theme::textSoft().name()));

    volLayout->addStretch();
    volLayout->addWidget(m_queueBtn, 0, Qt::AlignVCenter);
    volLayout->addWidget(m_volIcon, 0, Qt::AlignVCenter);
    volLayout->addWidget(m_volumeSlider, 0, Qt::AlignVCenter);

    m_rightWidget = new QWidget(this);
    m_rightWidget->setLayout(volLayout);
    m_rightWidget->setFixedWidth(196);
    lumen::design::StyleSheet::apply(m_rightWidget, QStringLiteral("background: transparent;"));
    mainLayout->addWidget(m_rightWidget);

    m_emptyLabel = new QLabel(Lang::tr("Adicione músicas para começar a ouvir"), this);
    m_emptyLabel->setFont(Theme::bodyFont(12));
    lumen::design::StyleSheet::apply(m_emptyLabel, QString(
        "color: %1; background: transparent;").arg(Theme::textMuted().name()));
    m_emptyLabel->setAlignment(Qt::AlignCenter);

    showEmptyUi();

    // --- Wire to engine ---
    connect(m_playBtn, &QPushButton::clicked, m_engine, &PlaybackEngine::togglePlay);
    connect(m_prevBtn, &QPushButton::clicked, m_engine, &PlaybackEngine::prev);
    connect(m_nextBtn, &QPushButton::clicked, m_engine, &PlaybackEngine::next);
    connect(m_shuffleBtn, &QPushButton::clicked, this, [this]() {
        m_engine->setShuffle(!m_engine->shuffle());
    });
    connect(m_repeatBtn, &QPushButton::clicked, m_engine, &PlaybackEngine::cycleRepeatMode);
    connect(m_queueBtn, &QPushButton::clicked, this, &PlayerBar::queueRequested);

    connect(m_progressSlider, &QSlider::sliderMoved, this, [this](int val) {
        const qint64 dur = m_engine->duration();
        if (dur > 0)
            m_engine->seek(dur * val / 1000);
    });
    connect(m_volIcon, &QPushButton::clicked, this, [this]() {
        m_engine->setMuted(!m_engine->isMuted());
    });
    connect(m_volumeSlider, &QSlider::sliderMoved, this, [this](int val) {
        m_engine->setVolume(val / 100.0);
        if (m_engine->isMuted())
            m_engine->setMuted(false);
    });

    connect(m_engine, &PlaybackEngine::trackChanged, this, [this](int id) {
        Q_UNUSED(id);
        const Track t = m_engine->currentTrack();
        if (t.id != 0)
            showTrackUi(t);
        else
            showEmptyUi();
        syncTransportUi();
        emit trackChanged(id);
    });
    connect(m_engine, &PlaybackEngine::playingChanged, this, [this](bool playing) {
        m_playBtn->setText(playing ? QStringLiteral("\uE103") : QStringLiteral("\uE102"));
        m_vinyl->setSpinning(playing);
        emit playingChanged(playing);
    });
    connect(m_engine, &PlaybackEngine::queueChanged, this, &PlayerBar::queueChanged);
    connect(m_engine, &PlaybackEngine::positionChanged, this, [this](qint64 pos) {
        m_timeLabel->setText(Theme::formatTime(pos));
        const qint64 dur = m_engine->duration();
        if (dur > 0 && !m_progressSlider->isSliderDown())
            m_progressSlider->setValue(static_cast<int>(pos * 1000 / dur));
    });
    connect(m_engine, &PlaybackEngine::durationChanged, this, [this](qint64 dur) {
        m_durationLabel->setText(Theme::formatTime(dur));
    });
    connect(m_engine, &PlaybackEngine::shuffleChanged, this, [this](bool on) {
        lumen::design::StyleSheet::apply(m_shuffleBtn, buttonStyle(on));
    });
    connect(m_engine, &PlaybackEngine::repeatModeChanged, this,
            [this](PlaybackEngine::RepeatMode mode) {
        const bool active = mode != PlaybackEngine::RepeatMode::Off;
        lumen::design::StyleSheet::apply(m_repeatBtn, buttonStyle(active));
        // Glyph hint: one vs all (same family; tooltips distinguish).
        if (mode == PlaybackEngine::RepeatMode::One)
            m_repeatBtn->setToolTip(Lang::tr("Repetir uma"));
        else if (mode == PlaybackEngine::RepeatMode::All)
            m_repeatBtn->setToolTip(Lang::tr("Repetir todas"));
        else
            m_repeatBtn->setToolTip(Lang::tr("Repetir"));
    });
    connect(m_engine, &PlaybackEngine::volumeChanged, this, [this](double v) {
        m_volumeSlider->setValue(static_cast<int>(v * 100));
        updateVolIcon();
    });
    connect(m_engine, &PlaybackEngine::mutedChanged, this, [this](bool) {
        updateVolIcon();
    });
}

void PlayerBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_emptyLabel) m_emptyLabel->setGeometry(rect());
}

void PlayerBar::showEmptyUi()
{
    m_emptyLabel->show();
    m_controlsContainer->hide();
    m_leftWidget->hide();
    m_rightWidget->hide();
}

void PlayerBar::showTrackUi(const Track &track)
{
    m_titleLabel->setText(track.title);
    m_artistLabel->setText(track.artist);
    m_vinyl->setGradient(track.cover);
    m_emptyLabel->hide();
    m_controlsContainer->show();
    m_leftWidget->show();
    m_rightWidget->show();
}

void PlayerBar::syncTransportUi()
{
    lumen::design::StyleSheet::apply(m_shuffleBtn, buttonStyle(m_engine->shuffle()));
    const bool rep = m_engine->repeatMode() != PlaybackEngine::RepeatMode::Off;
    lumen::design::StyleSheet::apply(m_repeatBtn, buttonStyle(rep));
    m_playBtn->setText(m_engine->isPlaying() ? QStringLiteral("\uE103")
                                             : QStringLiteral("\uE102"));
    m_vinyl->setSpinning(m_engine->isPlaying());
    m_volumeSlider->setValue(static_cast<int>(m_engine->volume() * 100));
    updateVolIcon();
}

// Facades -------------------------------------------------------------------

void PlayerBar::playTrack(const Track &t, const QList<Track> &q) { m_engine->playTrack(t, q); }
void PlayerBar::playKeepingContext(const Track &t) { m_engine->playKeepingContext(t); }
void PlayerBar::togglePlay() { m_engine->togglePlay(); }
void PlayerBar::next() { m_engine->next(); }
void PlayerBar::prev() { m_engine->prev(); }
void PlayerBar::enqueue(const Track &t) { m_engine->enqueue(t); }
void PlayerBar::removeFromQueue(int i) { m_engine->removeFromQueue(i); }
bool PlayerBar::takeFromQueue(int i, Track &o) { return m_engine->takeFromQueue(i, o); }
QList<Track> PlayerBar::userQueue() const { return m_engine->userQueue(); }
QList<Track> PlayerBar::upcomingContext() const { return m_engine->upcomingContext(); }
Track PlayerBar::currentTrack() const { return m_engine->currentTrack(); }
bool PlayerBar::isPlaying() const { return m_engine->isPlaying(); }
int  PlayerBar::currentTrackId() const { return m_engine->currentTrackId(); }
void PlayerBar::persistState() { m_engine->persistState(); }
void PlayerBar::restoreSession()
{
    m_engine->restoreSession();
    if (m_engine->currentTrackId() != 0)
        showTrackUi(m_engine->currentTrack());
    else
        showEmptyUi();
    syncTransportUi();
}

void PlayerBar::updateVolIcon()
{
    const bool muted = m_engine->isMuted();
    m_volIcon->setText(muted ? QStringLiteral("\uE198") : QStringLiteral("\uE15D"));
    m_volIcon->setToolTip(muted ? Lang::tr("Ativar som") : Lang::tr("Silenciar"));
}

QString PlayerBar::buttonStyle(bool active) const
{
    const QString color = active ? Theme::accent().name() : Theme::textMuted().name();
    const QString hover = active ? Theme::accent().lighter(110).name() : Theme::textSoft().name();
    return QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 16px; "
        "font-size: 14px; font-family: \"Segoe MDL2 Assets\"; }"
        "QPushButton:hover { color: %2; background: rgba(255,255,255,0.05); }"
    ).arg(color, hover);
}

QString PlayerBar::sliderStyle(const QString &accentColor) const
{
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
        QSlider::handle:horizontal:hover { background: #f0ece4; }
        QSlider::sub-page:horizontal {
            background: %1;
            border-radius: 2px;
        }
    )").arg(accentColor, Theme::text().name());
}
