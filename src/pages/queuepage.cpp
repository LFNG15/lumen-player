#include "design/stylesheet.h"
#include "queuepage.h"
#include "lang.h"
#include "theme.h"
#include "playerbar.h"

#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QResizeEvent>
#include <QShowEvent>
#include <QFontMetrics>
#include <QSizePolicy>

// Spotify-style queue side panel: a resizable column on the right of the window,
// toggled by the player bar's queue button (not a stacked page).
QueuePage::QueuePage(TrackModel *model, PlayerBar *player, QWidget *parent)
    : QWidget(parent), m_model(model), m_player(player)
{
    setObjectName(QStringLiteral("queuePanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumWidth(180);
    setMaximumWidth(480);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    lumen::design::StyleSheet::apply(this, QString(
        "QWidget#queuePanel { background: %1; border-left: 1px solid %2; }"
    ).arg(Theme::surface().name(), Theme::border().name()));

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_header = new QWidget(this);
    lumen::design::StyleSheet::apply(m_header, QStringLiteral("background: transparent;"));
    auto *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(16, 14, 10, 6);
    headerLayout->setSpacing(8);

    m_headerTitle = new QLabel(Lang::tr("Fila"), m_header);
    m_headerTitle->setFont(Theme::titleFont(15));
    m_headerTitle->setMinimumWidth(0);
    m_headerTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    lumen::design::StyleSheet::apply(m_headerTitle, QString(
        "color: %1; background: transparent;").arg(Theme::text().name()));
    headerLayout->addWidget(m_headerTitle, 1);

    auto *closeBtn = new QPushButton(QStringLiteral("\uE10A"), m_header);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFont(Theme::iconFont(11));
    closeBtn->setToolTip(Lang::tr("Fechar"));
    lumen::design::StyleSheet::apply(closeBtn, QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 14px; "
        "font-family: \"Segoe MDL2 Assets\"; }"
        "QPushButton:hover { background-color: %2; color: %3; }"
    ).arg(Theme::textMuted().name(), Theme::accent().name(), Theme::onAccent().name()));
    connect(closeBtn, &QPushButton::clicked, this, &QueuePage::navigateBack);
    headerLayout->addWidget(closeBtn);

    outerLayout->addWidget(m_header);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lumen::design::StyleSheet::apply(m_scroll, QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"));

    auto *content = new QWidget();
    lumen::design::StyleSheet::apply(content, QStringLiteral("background: transparent;"));
    m_contentLayout = new QVBoxLayout(content);
    m_contentLayout->setContentsMargins(10, 4, 10, 12);
    m_contentLayout->setSpacing(6);

    m_scroll->setWidget(content);
    outerLayout->addWidget(m_scroll, 1);
}

void QueuePage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    applyResponsiveLayout();
}

void QueuePage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    applyResponsiveLayout();
}

void QueuePage::applyResponsiveLayout()
{
    const int w = width();
    // Tighter chrome when the panel is dragged narrow.
    const int hPad = w < 220 ? 6 : (w < 280 ? 8 : 10);
    const int titlePx = w < 220 ? 13 : 15;
    if (m_headerTitle) {
        QFont f = Theme::titleFont(titlePx);
        m_headerTitle->setFont(f);
        m_headerTitle->setText(QFontMetrics(f).elidedText(
            Lang::tr("Fila"), Qt::ElideRight, qMax(40, w - 56)));
    }
    if (m_header) {
        if (auto *lay = qobject_cast<QHBoxLayout *>(m_header->layout()))
            lay->setContentsMargins(hPad + 6, 12, 8, 4);
    }
    if (m_contentLayout)
        m_contentLayout->setContentsMargins(hPad, 4, hPad, 12);
}

int QueuePage::coverSize() const
{
    const int w = width();
    if (w < 200) return 32;
    if (w < 260) return 36;
    return 40;
}

int QueuePage::rowHeight() const
{
    return coverSize() + 14;
}

void QueuePage::refresh(int currentTrackId, bool isPlaying)
{
    m_lastCurrentId = currentTrackId;
    m_lastPlaying = isPlaying;

    QLayoutItem *item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    Track current = m_player->currentTrack();
    QList<Track> userQueue = m_player->userQueue();
    QList<Track> upcoming   = m_player->upcomingContext();

    if (current.id == 0 && userQueue.isEmpty() && upcoming.isEmpty()) {
        auto *empty = new QLabel(Lang::tr(
            "A fila está vazia\nReproduza uma música ou adicione faixas à fila"));
        empty->setFont(Theme::bodyFont(12));
        empty->setWordWrap(true);
        lumen::design::StyleSheet::apply(empty, QString(
            "color: %1; background: transparent; padding-top: 30px;"
        ).arg(Theme::textMuted().name()));
        empty->setAlignment(Qt::AlignCenter);
        m_contentLayout->addWidget(empty);
        m_contentLayout->addStretch();
        applyResponsiveLayout();
        return;
    }

    auto addSectionLabel = [this](const QString &text) {
        auto *label = new QLabel(text);
        label->setFont(Theme::bodyFont(10));
        lumen::design::StyleSheet::apply(label, QString(
            "color: %1; background: transparent; font-weight: bold; "
            "letter-spacing: 1px; padding: 8px 6px 2px;"
        ).arg(Theme::textMuted().name()));
        m_contentLayout->addWidget(label);
    };

    if (current.id != 0) {
        addSectionLabel(Lang::tr("TOCANDO AGORA"));
        m_contentLayout->addWidget(createRow(current, QString(), true, -1));
    }

    if (!userQueue.isEmpty()) {
        addSectionLabel(Lang::tr("PRÓXIMAS NA FILA"));
        for (int i = 0; i < userQueue.size(); ++i)
            m_contentLayout->addWidget(createRow(userQueue[i], QString::number(i + 1), false, i));
    }

    if (!upcoming.isEmpty()) {
        addSectionLabel(Lang::tr("A SEGUIR"));
        for (int i = 0; i < upcoming.size(); ++i)
            m_contentLayout->addWidget(createRow(upcoming[i], QString(),
                                                 upcoming[i].id == currentTrackId, -1));
    }

    m_contentLayout->addStretch();
    applyResponsiveLayout();
}

QWidget *QueuePage::createRow(const Track &track, const QString &position, bool active,
                              int queueIndex)
{
    Q_UNUSED(position);
    const int cov = coverSize();
    const int rh = rowHeight();

    // Outer shell + padded inner button so accent wash isn't edge-to-edge.
    auto *shell = new QWidget();
    shell->setFixedHeight(rh + 6);
    shell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    lumen::design::StyleSheet::apply(shell, QStringLiteral("background: transparent;"));
    auto *shellLay = new QVBoxLayout(shell);
    shellLay->setContentsMargins(6, 3, 6, 3);
    shellLay->setSpacing(0);

    auto *row = new QPushButton(shell);
    row->setCursor(Qt::PointingHandCursor);
    row->setFlat(true);
    row->setFixedHeight(rh);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row->setMinimumWidth(0);

    // Translucent accent wash; labels stay light on active+hover.
    if (active) {
        lumen::design::StyleSheet::apply(row, QString(
            "QPushButton { background-color: %1; border: none; border-radius: 8px; text-align: left; }"
            "QPushButton:hover { background-color: %2; }"
        ).arg(Theme::accentRgba(0.32), Theme::accentRgba(0.40)));
    } else {
        lumen::design::StyleSheet::apply(row, QString(
            "QPushButton { background: transparent; border: none; border-radius: 8px; text-align: left; }"
            "QPushButton:hover { background-color: %1; }"
        ).arg(Theme::accentRgba(0.22)));
    }
    shellLay->addWidget(row);

    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(8);

    auto *swatch = new QWidget(row);
    swatch->setFixedSize(cov, cov);
    swatch->setAttribute(Qt::WA_TransparentForMouseEvents);
    lumen::design::StyleSheet::apply(swatch, QString(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 6px;"
    ).arg(track.cover.c1.name(), track.cover.c2.name()));
    layout->addWidget(swatch);

    // Dynamic colors: idle = theme text; active/hover = onAccent (white).
    // Hover of inactive rows: children keep soft colors unless we use a dynamic
    // property; approximate with active styling on click path. For hover of
    // inactive, paint onAccent always when parent is hovered via event filter
    // is heavy — use active colors when active, else text; CSS :hover on parent
    // cannot recolor QLabel children. Set dual-state via palette onAccent when
    // active; for hover of inactive use lighter approach: default text, on hover
    // the solid accent bg + we force labels to onAccent always on accent bg
    // by installing enter/leave.
    const QString titleCol = active ? Theme::onAccent().name() : Theme::text().name();
    const QString artistCol = active ? Theme::onAccent().name() : Theme::textSoft().name();

    auto *infoCol = new QVBoxLayout();
    infoCol->setSpacing(2);
    infoCol->setContentsMargins(0, 0, 0, 0);

    auto *titleLabel = new QLabel(track.title, row);
    titleLabel->setFont(Theme::bodyFont(11));
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleLabel->setMinimumWidth(0);
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    titleLabel->setText(QFontMetrics(titleLabel->font()).elidedText(
        track.title, Qt::ElideRight, qMax(40, width() - cov - 80)));
    lumen::design::StyleSheet::apply(titleLabel, QString(
        "color: %1; background: transparent; font-weight: 600;").arg(titleCol));

    auto *artistLabel = new QLabel(track.artist, row);
    artistLabel->setFont(Theme::bodyFont(10));
    artistLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    artistLabel->setMinimumWidth(0);
    artistLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    artistLabel->setText(QFontMetrics(artistLabel->font()).elidedText(
        track.artist, Qt::ElideRight, qMax(40, width() - cov - 80)));
    lumen::design::StyleSheet::apply(artistLabel, QString(
        "color: %1; background: transparent;").arg(artistCol));

    // Hover: recolor labels to onAccent when row is hovered (inactive rows).
    if (!active) {
        row->installEventFilter(this);
    }

    infoCol->addWidget(titleLabel);
    infoCol->addWidget(artistLabel);
    layout->addLayout(infoCol, 1);

    if (queueIndex >= 0) {
        auto *removeBtn = new QPushButton(QStringLiteral("\uE711"), row);
        removeBtn->setFixedSize(26, 26);
        removeBtn->setCursor(Qt::PointingHandCursor);
        removeBtn->setFont(Theme::iconFont(10));
        removeBtn->setToolTip(Lang::tr("Remover da fila"));
        const QString remIdle = active ? Theme::onAccent().name() : Theme::textMuted().name();
        lumen::design::StyleSheet::apply(removeBtn, QString(
            "QPushButton { background: transparent; color: %1; border: none; "
            "font-family: \"Segoe MDL2 Assets\"; }"
            "QPushButton:hover { color: %2; }"
        ).arg(remIdle, Theme::danger().name()));
        connect(removeBtn, &QPushButton::clicked, this, [this, queueIndex]() {
            emit removeFromQueueRequested(queueIndex);
        });
        layout->addWidget(removeBtn);
    }

    if (queueIndex >= 0)
        connect(row, &QPushButton::clicked, this, [this, queueIndex]() {
            emit playFromQueue(queueIndex);
        });
    else if (!active)
        connect(row, &QPushButton::clicked, this, [this, track]() {
            emit playContext(track);
        });

    // Enter/leave: flip label colors to onAccent while hovering inactive rows.
    if (!active) {
        const QString onA = Theme::onAccent().name();
        const QString idleT = Theme::text().name();
        const QString idleA = Theme::textSoft().name();
        row->setProperty("titleIdle", idleT);
        row->setProperty("artistIdle", idleA);
        row->setProperty("onAccent", onA);
        // Labels found via findChild in eventFilter (object names).
        titleLabel->setObjectName(QStringLiteral("queueTitle"));
        artistLabel->setObjectName(QStringLiteral("queueArtist"));
    }

    return shell;
}

// Event filter for queue row hover label recolor (inactive rows).
bool QueuePage::eventFilter(QObject *obj, QEvent *event)
{
    auto *btn = qobject_cast<QPushButton *>(obj);
    if (!btn)
        return QWidget::eventFilter(obj, event);

    auto *title = btn->findChild<QLabel *>(QStringLiteral("queueTitle"));
    auto *artist = btn->findChild<QLabel *>(QStringLiteral("queueArtist"));
    if (!title || !artist)
        return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::Enter) {
        const QString onA = btn->property("onAccent").toString();
        lumen::design::StyleSheet::apply(title, QString(
            "color: %1; background: transparent; font-weight: 600;").arg(onA));
        lumen::design::StyleSheet::apply(artist, QString(
            "color: %1; background: transparent;").arg(onA));
    } else if (event->type() == QEvent::Leave) {
        lumen::design::StyleSheet::apply(title, QString(
            "color: %1; background: transparent; font-weight: 600;")
            .arg(btn->property("titleIdle").toString()));
        lumen::design::StyleSheet::apply(artist, QString(
            "color: %1; background: transparent;")
            .arg(btn->property("artistIdle").toString()));
    }
    return QWidget::eventFilter(obj, event);
}
