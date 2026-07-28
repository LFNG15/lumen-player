#include "searchpage.h"
#include "lang.h"
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QFrame>
#include "hoverplayfilter.h"
#include "theme.h"
#include "design/stylesheet.h"
#include "coverwidget.h"
#include "textutils.h"

static bool matches(const QString &haystack, const QString &needle) {
    return TextUtils::normalized(haystack).contains(needle);
}

SearchPage::SearchPage(TrackModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    lumen::design::StyleSheet::apply(scroll, "QScrollArea { background: transparent; border: none; }");

    auto *content = new QWidget();
    lumen::design::StyleSheet::apply(content, "background: transparent;");
    m_contentLayout = new QVBoxLayout(content);
    m_contentLayout->setContentsMargins(32, 28, 32, 28);
    m_contentLayout->setSpacing(12);

    scroll->setWidget(content);
    outerLayout->addWidget(scroll);
}

void SearchPage::setQuery(const QString &query) {
    m_query = query.trimmed();
}

void SearchPage::refresh(int currentTrackId, bool /*isPlaying*/) {
    QLayoutItem *item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (m_query.isEmpty()) {
        auto *hint = new QLabel(Lang::tr("Digite algo na busca para encontrar músicas e playlists"));
        hint->setFont(Theme::bodyFont(14));
        lumen::design::StyleSheet::apply(hint, QString("color: %1; background: transparent; padding-top: 60px;").arg(Theme::textMuted().name()));
        hint->setAlignment(Qt::AlignCenter);
        m_contentLayout->addWidget(hint);
        m_contentLayout->addStretch();
        return;
    }

    const QString needle = TextUtils::normalized(m_query);

    // Matching playlists (by name)
    QList<Folder> folderHits;
    for (const auto &f : m_model->folders()) {
        if (matches(f.name, needle)) folderHits.append(f);
    }

    // Matching tracks (title, artist or playlist name)
    QList<Track> trackHits;
    for (const auto &t : m_model->tracks()) {
        if (matches(t.title, needle) || matches(t.artist, needle) || matches(t.folder, needle))
            trackHits.append(t);
    }

    auto *title = new QLabel(QString(Lang::tr("Resultados para “%1”")).arg(m_query));
    title->setFont(Theme::titleFont(24));
    lumen::design::StyleSheet::apply(title, QString("color: %1; background: transparent; padding-bottom: 8px;").arg(Theme::text().name()));
    m_contentLayout->addWidget(title);

    if (folderHits.isEmpty() && trackHits.isEmpty()) {
        auto *empty = new QLabel(QString(Lang::tr("Nenhum resultado para “%1”\nVerifique a escrita ou tente outras palavras")).arg(m_query));
        empty->setFont(Theme::bodyFont(14));
        lumen::design::StyleSheet::apply(empty, QString("color: %1; background: transparent; padding-top: 40px;").arg(Theme::textMuted().name()));
        empty->setAlignment(Qt::AlignCenter);
        m_contentLayout->addWidget(empty);
        m_contentLayout->addStretch();
        return;
    }

    if (!folderHits.isEmpty()) {
        auto *playlistsLabel = new QLabel(Lang::tr("Playlists"));
        playlistsLabel->setFont(Theme::titleFont(18));
        lumen::design::StyleSheet::apply(playlistsLabel, QString("color: %1; background: transparent; padding-top: 4px;").arg(Theme::text().name()));
        m_contentLayout->addWidget(playlistsLabel);

        auto *grid = new QGridLayout();
        grid->setSpacing(8);
        int col = 0, row = 0;
        for (const auto &f : folderHits) {
            int count = m_model->tracksInFolder(f.name).size();
            grid->addWidget(createPlaylistCard(f, count), row, col);
            if (++col >= 3) { col = 0; row++; }
        }
        // Keep cards from stretching across the page when there are few hits.
        grid->setColumnStretch(3, 1);
        auto *gridWidget = new QWidget();
        gridWidget->setLayout(grid);
        lumen::design::StyleSheet::apply(gridWidget, "background: transparent;");
        m_contentLayout->addWidget(gridWidget);
        m_contentLayout->addSpacing(12);
    }

    if (!trackHits.isEmpty()) {
        auto *tracksLabel = new QLabel(Lang::tr("Músicas"));
        tracksLabel->setFont(Theme::titleFont(18));
        lumen::design::StyleSheet::apply(tracksLabel, QString("color: %1; background: transparent; padding-top: 4px;").arg(Theme::text().name()));
        m_contentLayout->addWidget(tracksLabel);

        for (int i = 0; i < trackHits.size(); ++i) {
            m_contentLayout->addWidget(createTrackRow(trackHits[i], i, currentTrackId));
        }
    }

    m_contentLayout->addStretch();
}

QWidget *SearchPage::createPlaylistCard(const Folder &folder, int trackCount) {
    auto *chip = new QPushButton();
    chip->setFixedSize(220, 64);
    chip->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(chip, QString(
        "QPushButton { background: %1; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: %2; }"
    ).arg(Theme::card().name(), Theme::cardHover().name()));

    auto *layout = new QHBoxLayout(chip);
    layout->setContentsMargins(8, 8, 12, 8);
    layout->setSpacing(10);

    layout->addWidget(CoverWidget::playlistCover(folder, m_model->tracksInFolder(folder.name), 48, 6),
                      0, Qt::AlignVCenter);

    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(1);
    auto *nameLabel = new QLabel(folder.name);
    nameLabel->setFont(Theme::bodyFont(13));
    lumen::design::StyleSheet::apply(nameLabel, QString("color: %1; background: transparent; font-weight: bold;").arg(Theme::text().name()));
    auto *countLabel = new QLabel(QString(Lang::tr("%1 faixa%2")).arg(trackCount).arg(trackCount != 1 ? "s" : ""));
    countLabel->setFont(Theme::bodyFont(10));
    lumen::design::StyleSheet::apply(countLabel, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    infoLayout->addStretch();
    infoLayout->addWidget(nameLabel);
    infoLayout->addWidget(countLabel);
    infoLayout->addStretch();
    layout->addLayout(infoLayout, 1);

    QString name = folder.name;
    connect(chip, &QPushButton::clicked, [this, name]() { emit navigateTo("folder", name); });
    return chip;
}

QWidget *SearchPage::createTrackRow(const Track &track, int index, int currentId) {
    bool active = (track.id == currentId);
    auto *row = new QWidget();
    row->setObjectName("trackRow");
    row->setFixedHeight(52);
    row->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(row, QString(
        "QWidget#trackRow { background: %1; border-radius: 8px; border-left: 3px solid %2; }"
    ).arg(
        active ? Theme::accentRgba(0.12) : QStringLiteral("transparent"),
        active ? Theme::accent().name() : "transparent"
    ));

    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 4, 12, 4);
    layout->setSpacing(12);

    QString idxNum = QString("%1").arg(index + 1, 2, 10, QChar('0'));
    auto *idx = new QLabel(active ? QStringLiteral("") : idxNum);
    idx->setFont(Theme::monoFont(12));
    idx->setFixedWidth(28);
    idx->setAlignment(Qt::AlignCenter);
    lumen::design::StyleSheet::apply(idx, QString("color: %1; background: transparent; font-family: \"Segoe MDL2 Assets\", Consolas;").arg(
        active ? Theme::accent().name() : Theme::textMuted().name()));
    idx->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(idx);

    auto *swatch = new QWidget();
    swatch->setFixedSize(38, 38);
    swatch->setAttribute(Qt::WA_TransparentForMouseEvents);
    lumen::design::StyleSheet::apply(swatch, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1, stop:1 %2); border-radius: 6px;")
        .arg(track.cover.c1.name(), track.cover.c2.name()));
    layout->addWidget(swatch);

    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(1);
    auto *title = new QLabel(track.title);
    title->setFont(Theme::bodyFont(13));
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    lumen::design::StyleSheet::apply(title, QString("color: %1; background: transparent; font-weight: 600;").arg(
        active ? Theme::accent().name() : Theme::text().name()));
    auto *artist = new QLabel(track.artist);
    artist->setFont(Theme::bodyFont(11));
    artist->setAttribute(Qt::WA_TransparentForMouseEvents);
    lumen::design::StyleSheet::apply(artist, QString("color: %1; background: transparent;").arg(Theme::textSoft().name()));
    infoLayout->addWidget(title);
    infoLayout->addWidget(artist);
    layout->addLayout(infoLayout, 1);

    if (!track.folder.isEmpty()) {
        auto *tag = new QPushButton(track.folder);
        tag->setFont(Theme::bodyFont(10));
        tag->setCursor(Qt::PointingHandCursor);
        lumen::design::StyleSheet::apply(tag, QString(
            "QPushButton { color: %1; background: " + Theme::accentRgba(0.10) + "; border: none; border-radius: 10px; padding: 2px 8px; }"
            "QPushButton:hover { background: " + Theme::accentRgba(0.28) + "; color: %2; }"
        ).arg(Theme::accentDim().name(), Theme::text().name()));
        QString folderName = track.folder;
        connect(tag, &QPushButton::clicked, [this, folderName]() { emit navigateTo("folder", folderName); });
        layout->addWidget(tag);
    }

    auto *queueBtn = new QPushButton(QStringLiteral(""));
    queueBtn->setFixedSize(28, 28);
    queueBtn->setCursor(Qt::PointingHandCursor);
    queueBtn->setToolTip(Lang::tr("Adicionar à fila"));
    lumen::design::StyleSheet::apply(queueBtn, QString(
        "QPushButton { background: transparent; color: %1; border: none; font-size: 13px; font-family: \"Segoe MDL2 Assets\"; }"
        "QPushButton:hover { color: %2; }"
    ).arg(Theme::textMuted().name(), Theme::accent().name()));
    Track qt = track;
    connect(queueBtn, &QPushButton::clicked, [this, qt]() { emit enqueueRequested(qt); });
    layout->addWidget(queueBtn);

    auto *likeBtn = new QPushButton(track.liked ? "" : "");
    likeBtn->setFixedSize(28, 28);
    likeBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(likeBtn, QString(
        "QPushButton { background: transparent; color: %1; border: none; font-size: 14px; font-family: \"Segoe MDL2 Assets\"; }"
        "QPushButton:hover { color: %2; }"
    ).arg(track.liked ? Theme::accent().name() : Theme::textMuted().name(), Theme::accent().name()));
    int likeId = track.id;
    connect(likeBtn, &QPushButton::clicked, [this, likeId]() { emit likeToggled(likeId); });
    layout->addWidget(likeBtn);

    auto *dur = new QLabel(Theme::formatTime(track.durationMs));
    dur->setFont(Theme::monoFont(11));
    dur->setFixedWidth(40);
    dur->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lumen::design::StyleSheet::apply(dur, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    layout->addWidget(dur);

    Track t = track;
    auto *overlay = new QPushButton(row);
    overlay->setGeometry(0, 0, 9999, 52);
    lumen::design::StyleSheet::apply(overlay, "background: transparent; border: none;");
    overlay->setCursor(Qt::PointingHandCursor);
    overlay->lower();
    connect(overlay, &QPushButton::clicked, [this, t]() { emit playRequested(t); });
    if (!active) overlay->installEventFilter(new HoverPlayFilter(idx, idxNum, overlay));

    return row;
}
