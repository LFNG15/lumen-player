#include "queuepage.h"
#include "lang.h"
#include "playerbar.h"
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>

// Spotify-style queue side panel: a fixed-width column on the right of the
// window, toggled by the player bar's queue button (it is not a stacked page).
QueuePage::QueuePage(TrackModel *model, PlayerBar *player, QWidget *parent)
    : QWidget(parent), m_model(model), m_player(player)
{
    setObjectName("queuePanel");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QString("QWidget#queuePanel { background: %1; border-left: 1px solid %2; }")
        .arg(Theme::surface().name(), Theme::border().name()));

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Header: title + close button
    auto *header = new QWidget();
    header->setStyleSheet("background: transparent;");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 14, 10, 6);
    headerLayout->setSpacing(8);

    auto *title = new QLabel(Lang::tr("Fila"));
    title->setFont(Theme::titleFont(15));
    title->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::text().name()));
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    auto *closeBtn = new QPushButton("");
    closeBtn->setFixedSize(28, 28);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFont(Theme::iconFont(11));
    closeBtn->setToolTip(Lang::tr("Fechar"));
    closeBtn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 14px; font-family: \"Segoe MDL2 Assets\"; }"
        "QPushButton:hover { background: rgba(255,255,255,0.08); color: %2; }"
    ).arg(Theme::textMuted().name(), Theme::text().name()));
    connect(closeBtn, &QPushButton::clicked, this, &QueuePage::navigateBack);
    headerLayout->addWidget(closeBtn);

    outerLayout->addWidget(header);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    auto *content = new QWidget();
    content->setStyleSheet("background: transparent;");
    m_contentLayout = new QVBoxLayout(content);
    m_contentLayout->setContentsMargins(10, 4, 10, 12);
    m_contentLayout->setSpacing(4);

    scroll->setWidget(content);
    outerLayout->addWidget(scroll, 1);
}

void QueuePage::refresh(int currentTrackId, bool /*isPlaying*/) {
    QLayoutItem *item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    Track current = m_player->currentTrack();
    QList<Track> userQueue = m_player->userQueue();
    QList<Track> upcoming   = m_player->upcomingContext();

    if (current.id == 0 && userQueue.isEmpty() && upcoming.isEmpty()) {
        auto *empty = new QLabel(Lang::tr("A fila está vazia\nReproduza uma música ou adicione faixas à fila"));
        empty->setFont(Theme::bodyFont(12));
        empty->setWordWrap(true);
        empty->setStyleSheet(QString("color: %1; background: transparent; padding-top: 30px;").arg(Theme::textMuted().name()));
        empty->setAlignment(Qt::AlignCenter);
        m_contentLayout->addWidget(empty);
        m_contentLayout->addStretch();
        return;
    }

    auto addSectionLabel = [this](const QString &text) {
        auto *label = new QLabel(text);
        label->setFont(Theme::bodyFont(10));
        label->setStyleSheet(QString("color: %1; background: transparent; font-weight: bold; letter-spacing: 1px; padding: 8px 6px 2px;")
            .arg(Theme::textMuted().name()));
        m_contentLayout->addWidget(label);
    };

    // Now playing
    if (current.id != 0) {
        addSectionLabel(Lang::tr("TOCANDO AGORA"));
        m_contentLayout->addWidget(createRow(current, QString(), true, -1));
    }

    // Manually queued tracks
    if (!userQueue.isEmpty()) {
        addSectionLabel(Lang::tr("PRÓXIMAS NA FILA"));
        for (int i = 0; i < userQueue.size(); ++i)
            m_contentLayout->addWidget(createRow(userQueue[i], QString::number(i + 1), false, i));
    }

    // Rest of the current context
    if (!upcoming.isEmpty()) {
        addSectionLabel(Lang::tr("A SEGUIR"));
        for (int i = 0; i < upcoming.size(); ++i)
            m_contentLayout->addWidget(createRow(upcoming[i], QString(), upcoming[i].id == currentTrackId, -1));
    }

    m_contentLayout->addStretch();
}

QWidget *QueuePage::createRow(const Track &track, const QString &position, bool active, int queueIndex) {
    Q_UNUSED(position);
    auto *row = new QWidget();
    row->setObjectName("trackRow");
    row->setFixedHeight(54);
    row->setCursor(Qt::PointingHandCursor);
    row->setStyleSheet(QString(
        "QWidget#trackRow { background: %1; border-radius: 8px; }"
    ).arg(active ? Theme::accentRgba(0.12) : QStringLiteral("transparent")));

    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 5, 8, 5);
    layout->setSpacing(10);

    auto *swatch = new QWidget();
    swatch->setFixedSize(40, 40);
    swatch->setAttribute(Qt::WA_TransparentForMouseEvents);
    swatch->setStyleSheet(QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 6px;")
        .arg(track.cover.c1.name(), track.cover.c2.name()));
    layout->addWidget(swatch);

    auto *infoCol = new QVBoxLayout();
    infoCol->setSpacing(1);
    auto *titleLabel = new QLabel(track.title);
    titleLabel->setFont(Theme::bodyFont(11));
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleLabel->setStyleSheet(QString("color: %1; background: transparent; font-weight: 600;")
        .arg(active ? Theme::accent().name() : Theme::text().name()));
    auto *artistLabel = new QLabel(track.artist);
    artistLabel->setFont(Theme::bodyFont(10));
    artistLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    artistLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::textSoft().name()));
    infoCol->addWidget(titleLabel);
    infoCol->addWidget(artistLabel);
    layout->addLayout(infoCol, 1);

    // Remove-from-queue button (only for manually queued items)
    if (queueIndex >= 0) {
        auto *removeBtn = new QPushButton("");
        removeBtn->setFixedSize(26, 26);
        removeBtn->setCursor(Qt::PointingHandCursor);
        removeBtn->setFont(Theme::iconFont(10));
        removeBtn->setToolTip(Lang::tr("Remover da fila"));
        removeBtn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; border: none; font-family: \"Segoe MDL2 Assets\"; }"
            "QPushButton:hover { color: %2; }"
        ).arg(Theme::textMuted().name(), Theme::danger().name()));
        connect(removeBtn, &QPushButton::clicked, [this, queueIndex]() { emit removeFromQueueRequested(queueIndex); });
        layout->addWidget(removeBtn);
    }

    // Click overlay to play (not for the currently-playing row)
    if (!active) {
        Track t = track;
        auto *overlay = new QPushButton(row);
        overlay->setGeometry(0, 0, 9999, 54);
        overlay->setStyleSheet("background: transparent; border: none;");
        overlay->setCursor(Qt::PointingHandCursor);
        overlay->lower();
        if (queueIndex >= 0)
            connect(overlay, &QPushButton::clicked, [this, queueIndex]() { emit playFromQueue(queueIndex); });
        else
            connect(overlay, &QPushButton::clicked, [this, t]() { emit playContext(t); });
    }

    return row;
}
