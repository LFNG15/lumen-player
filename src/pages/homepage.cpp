#include "design/stylesheet.h"
#include "homepage.h"
#include "lang.h"
#include "models/trackcontextmenu.h"
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QPainter>
#include <QLinearGradient>
#include <QDateTime>
#include <QFrame>
#include <QCursor>
#include "hoverplayfilter.h"
#include "coverwidget.h"

HomePage::HomePage(TrackModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    m_ctx = new TrackContextMenu(m_model, this);
    connect(m_ctx, &TrackContextMenu::enqueueRequested, this, &HomePage::enqueueRequested);
    connect(m_ctx, &TrackContextMenu::playRequested, this, &HomePage::playRequested);
    connect(m_ctx, &TrackContextMenu::editRequested, this, &HomePage::editTrackRequested);
    connect(m_ctx, &TrackContextMenu::deleteRequested, this, &HomePage::deleteRequested);
    connect(m_ctx, &TrackContextMenu::likeToggled, this, &HomePage::likeToggled);
    connect(m_ctx, &TrackContextMenu::membershipChanged, this, [this]() {
        // Playlist membership changed — soft refresh of current home view.
        // Parent MainWindow also refreshes on tracksChanged for most edits.
    });

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    lumen::design::StyleSheet::apply(m_scroll, "QScrollArea { background: transparent; border: none; }");

    auto *content = new QWidget();
    lumen::design::StyleSheet::apply(content, "background: transparent;");
    m_contentLayout = new QVBoxLayout(content);
    m_contentLayout->setContentsMargins(32, 28, 32, 28);
    m_contentLayout->setSpacing(12);

    m_scroll->setWidget(content);
    outerLayout->addWidget(m_scroll);
}

void HomePage::refresh(int currentTrackId, bool isPlaying) {
    // Clear existing
    QLayoutItem *item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        if (item->layout()) {
            QLayoutItem *sub;
            while ((sub = item->layout()->takeAt(0)) != nullptr) {
                if (sub->widget()) sub->widget()->deleteLater();
                delete sub;
            }
        }
        delete item;
    }

    // Greeting
    int hour = QTime::currentTime().hour();
    QString greeting = hour < 12 ? Lang::tr("Bom dia") : (hour < 18 ? Lang::tr("Boa tarde") : Lang::tr("Boa noite"));
    auto *greetLabel = new QLabel(greeting);
    greetLabel->setFont(Theme::titleFont(28));
    lumen::design::StyleSheet::apply(greetLabel, QString("color: %1; background: transparent; padding-bottom: 8px;").arg(Theme::text().name()));
    m_contentLayout->addWidget(greetLabel);

    if (m_model->tracks().isEmpty()) {
        // Empty state
        auto *emptyWidget = new QWidget();
        lumen::design::StyleSheet::apply(emptyWidget, "background: transparent;");
        auto *emptyLayout = new QVBoxLayout(emptyWidget);
        emptyLayout->setAlignment(Qt::AlignCenter);
        emptyLayout->setSpacing(12);

        auto *msg = new QLabel(Lang::tr("Sua biblioteca está vazia"));
        msg->setFont(Theme::bodyFont(16));
        lumen::design::StyleSheet::apply(msg, QString("color: %1; background: transparent;").arg(Theme::textSoft().name()));
        msg->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(msg);

        auto *sub = new QLabel(Lang::tr("Adicione seus arquivos de áudio para começar"));
        sub->setFont(Theme::bodyFont(12));
        lumen::design::StyleSheet::apply(sub, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
        sub->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(sub);

        auto *addBtn = new QPushButton(Lang::tr("Adicionar Músicas"));
        addBtn->setFont(Theme::bodyFont(14));
        addBtn->setFixedSize(200, 44);
        addBtn->setCursor(Qt::PointingHandCursor);
        lumen::design::StyleSheet::apply(addBtn, QString(
            "QPushButton { background-color: %1; color: %2; border: none; border-radius: 22px; font-weight: bold; }"
            "QPushButton:hover { background-color: %3; }"
        ).arg(Theme::accent().name(), Theme::bg().name(), Theme::accent().lighter(110).name()));
        connect(addBtn, &QPushButton::clicked, [this]() { emit navigateTo("add"); });
        emptyLayout->addWidget(addBtn, 0, Qt::AlignCenter);

        emptyWidget->setMinimumHeight(350);
        m_contentLayout->addWidget(emptyWidget);
        m_contentLayout->addStretch();
        return;
    }

    // ── Folder chips ────────────────────────────────────────
    auto folders = m_model->folders();
    if (!folders.isEmpty()) {
        auto *grid = new QGridLayout();
        grid->setSpacing(8);
        int col = 0, row = 0;
        for (auto &f : folders) {
            int count = m_model->tracksInFolder(f.name).size();
            auto *chip = createFolderChip(f, count);
            grid->addWidget(chip, row, col);
            col++;
            if (col >= 3) { col = 0; row++; }
        }
        // Liked chip — same layout as playlist chips, with a heart cover.
        auto liked = m_model->likedTracks();
        if (!liked.isEmpty()) {
            auto *likedChip = new QPushButton();
            likedChip->setFixedHeight(64);
            likedChip->setCursor(Qt::PointingHandCursor);
            lumen::design::StyleSheet::apply(likedChip, QString(
                "QPushButton { background: %1; border: none; border-radius: 8px; }"
                "QPushButton:hover { background: %2; }"
            ).arg(Theme::card().name(), Theme::cardHover().name()));

            auto *likedLayout = new QHBoxLayout(likedChip);
            likedLayout->setContentsMargins(8, 8, 12, 8);
            likedLayout->setSpacing(10);

            auto *heartCover = new QLabel("");
            heartCover->setFixedSize(48, 48);
            heartCover->setAlignment(Qt::AlignCenter);
            heartCover->setFont(Theme::iconFont(18));
            heartCover->setAttribute(Qt::WA_TransparentForMouseEvents);
            lumen::design::StyleSheet::apply(heartCover, QString(
                "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2);"
                "color: %3; border-radius: 6px;")
                .arg(Theme::accent().name(), Theme::accentDim().darker(160).name(), Theme::text().name()));
            likedLayout->addWidget(heartCover, 0, Qt::AlignVCenter);

            auto *likedInfo = new QVBoxLayout();
            likedInfo->setSpacing(1);
            auto *likedName = new QLabel(Lang::tr("Curtidas"));
            likedName->setFont(Theme::bodyFont(13));
            lumen::design::StyleSheet::apply(likedName, QString("color: %1; background: transparent; font-weight: bold;").arg(Theme::text().name()));
            auto *likedCount = new QLabel(QString(Lang::tr("%1 faixa%2")).arg(liked.size()).arg(liked.size() != 1 ? "s" : ""));
            likedCount->setFont(Theme::bodyFont(10));
            lumen::design::StyleSheet::apply(likedCount, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
            likedInfo->addStretch();
            likedInfo->addWidget(likedName);
            likedInfo->addWidget(likedCount);
            likedInfo->addStretch();
            likedLayout->addLayout(likedInfo, 1);

            connect(likedChip, &QPushButton::clicked, [this]() { emit navigateTo("liked"); });
            grid->addWidget(likedChip, row, col);
        }
        auto *gridWidget = new QWidget();
        gridWidget->setLayout(grid);
        lumen::design::StyleSheet::apply(gridWidget, "background: transparent;");
        m_contentLayout->addWidget(gridWidget);
        m_contentLayout->addSpacing(16);
    }

    // ── Recents: last played playlists (Spotify-style cards) ─
    auto recentFolders = m_model->recentlyPlayedFolders(6);
    if (!recentFolders.isEmpty()) {
        auto *recentsLabel = new QLabel(Lang::tr("Recentes"));
        recentsLabel->setFont(Theme::titleFont(18));
        lumen::design::StyleSheet::apply(recentsLabel, QString("color: %1; background: transparent; padding-top: 8px;").arg(Theme::text().name()));
        m_contentLayout->addWidget(recentsLabel);

        auto *cardsRow = new QHBoxLayout();
        cardsRow->setSpacing(12);
        cardsRow->setContentsMargins(0, 0, 0, 0);
        for (const auto &f : recentFolders)
            cardsRow->addWidget(createRecentCard(f));
        cardsRow->addStretch();

        auto *rowWidget = new QWidget();
        rowWidget->setLayout(cardsRow);
        lumen::design::StyleSheet::apply(rowWidget, "background: transparent;");

        // Horizontal strip; extra cards scroll with the mouse wheel (the
        // global style hides horizontal scrollbars).
        auto *strip = new QScrollArea();
        strip->setWidgetResizable(true);
        strip->setFrameShape(QFrame::NoFrame);
        strip->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        lumen::design::StyleSheet::apply(strip, "QScrollArea { background: transparent; border: none; }");
        strip->setFixedHeight(206);
        strip->setWidget(rowWidget);
        m_contentLayout->addWidget(strip);
        m_contentLayout->addSpacing(16);
    }

    // ── Recently played (listening history) ─────────────────
    auto played = m_model->recentlyPlayed(8);
    if (!played.isEmpty()) {
        auto *playedLabel = new QLabel(Lang::tr("Tocadas recentemente"));
        playedLabel->setFont(Theme::titleFont(18));
        lumen::design::StyleSheet::apply(playedLabel, QString("color: %1; background: transparent; padding-top: 8px;").arg(Theme::text().name()));
        m_contentLayout->addWidget(playedLabel);

        for (int i = 0; i < played.size(); ++i) {
            m_contentLayout->addWidget(createTrackRow(played[i], i, currentTrackId, isPlaying));
        }
        m_contentLayout->addSpacing(16);
    }

    // ── Recent tracks ───────────────────────────────────────
    auto recent = m_model->recentTracks(8);
    if (!recent.isEmpty()) {
        auto *recentLabel = new QLabel(Lang::tr("Adicionadas recentemente"));
        recentLabel->setFont(Theme::titleFont(18));
        lumen::design::StyleSheet::apply(recentLabel, QString("color: %1; background: transparent; padding-top: 8px;").arg(Theme::text().name()));
        m_contentLayout->addWidget(recentLabel);

        for (int i = 0; i < recent.size(); ++i) {
            m_contentLayout->addWidget(createTrackRow(recent[i], i, currentTrackId, isPlaying));
        }
        m_contentLayout->addSpacing(16);
    }

    // ── All tracks ──────────────────────────────────────────
    auto *allLabel = new QLabel(Lang::tr("Biblioteca Completa"));
    allLabel->setFont(Theme::titleFont(18));
    lumen::design::StyleSheet::apply(allLabel, QString("color: %1; background: transparent; padding-top: 8px;").arg(Theme::text().name()));
    m_contentLayout->addWidget(allLabel);

    auto &all = m_model->tracks();
    for (int i = 0; i < all.size(); ++i) {
        m_contentLayout->addWidget(createTrackRow(all[i], i, currentTrackId, isPlaying));
    }
    m_contentLayout->addStretch();
}

QWidget *HomePage::createTrackRow(const Track &track, int index, int currentId, bool isPlaying) {
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

    // Index — shows a play glyph on hover (and on the active row)
    QString idxNum = QString("%1").arg(index + 1, 2, 10, QChar('0'));
    auto *idx = new QLabel(active ? QStringLiteral("\uE102") : idxNum);
    idx->setFont(Theme::monoFont(12));
    idx->setFixedWidth(28);
    idx->setAlignment(Qt::AlignCenter);
    lumen::design::StyleSheet::apply(idx, QString("color: %1; background: transparent; font-family: \"Segoe MDL2 Assets\", Consolas;").arg(
        active ? Theme::accent().name() : Theme::textMuted().name()));
    idx->setAttribute(Qt::WA_TransparentForMouseEvents);  // let clicks reach the play overlay
    layout->addWidget(idx);

    // Cover swatch
    auto *swatch = new QWidget();
    swatch->setFixedSize(38, 38);
    swatch->setAttribute(Qt::WA_TransparentForMouseEvents);
    lumen::design::StyleSheet::apply(swatch, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1, stop:1 %2); border-radius: 6px;")
        .arg(track.cover.c1.name(), track.cover.c2.name()));
    layout->addWidget(swatch);

    // Info
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

    // Folder tag — clickable, navigates to the playlist
    if (!track.folder.isEmpty()) {
        auto *tag = new QPushButton(track.folder);
        tag->setFont(Theme::bodyFont(10));
        tag->setCursor(Qt::PointingHandCursor);
        tag->setToolTip(QString(Lang::tr("Ir para a playlist \"%1\"")).arg(track.folder));
        lumen::design::StyleSheet::apply(tag, QString(
            "QPushButton { color: %1; background: " + Theme::accentRgba(0.10) + "; border: none; border-radius: 10px; padding: 2px 8px; }"
            "QPushButton:hover { background: " + Theme::accentRgba(0.28) + "; color: %2; }"
        ).arg(Theme::accentDim().name(), Theme::text().name()));
        QString folderName = track.folder;
        connect(tag, &QPushButton::clicked, [this, folderName]() { emit navigateTo("folder", folderName); });
        layout->addWidget(tag);
    }

    // "+" → choose queue or playlist (not enqueue-only).
    auto *addBtn = new QPushButton(QStringLiteral("\uE710"));
    addBtn->setFixedSize(28, 28);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setToolTip(Lang::tr("Adicionar à fila ou playlist"));
    lumen::design::StyleSheet::apply(addBtn, QString(
        "QPushButton { background: transparent; color: %1; border: none; font-size: 13px; font-family: \"Segoe MDL2 Assets\"; }"
        "QPushButton:hover { color: %2; }"
    ).arg(Theme::textMuted().name(), Theme::accent().name()));
    const int addId = track.id;
    connect(addBtn, &QPushButton::clicked, this, [this, addId]() {
        m_ctx->popupAddMenu({addId}, QCursor::pos());
    });
    layout->addWidget(addBtn);

    // Like button
    auto *likeBtn = new QPushButton(track.liked ? "\uE00B" : "\uE006");
    likeBtn->setFixedSize(28, 28);
    likeBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(likeBtn, QString(
        "QPushButton { background: transparent; color: %1; border: none; font-size: 14px; font-family: \"Segoe MDL2 Assets\"; }"
        "QPushButton:hover { color: %2; }"
    ).arg(track.liked ? Theme::accent().name() : Theme::textMuted().name(), Theme::accent().name()));
    int likeId = track.id;
    connect(likeBtn, &QPushButton::clicked, [this, likeId]() { emit likeToggled(likeId); });
    layout->addWidget(likeBtn);

    // Edit info button
    auto *editBtn = new QPushButton(QStringLiteral("\uE70F"));
    editBtn->setFixedSize(28, 28);
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setFont(Theme::iconFont(11));
    editBtn->setToolTip(Lang::tr("Editar música"));
    lumen::design::StyleSheet::apply(editBtn, QString(
        "QPushButton { background: transparent; color: %1; border: none; font-family: \"Segoe MDL2 Assets\"; }"
        "QPushButton:hover { color: %2; }"
    ).arg(Theme::textMuted().name(), Theme::accent().name()));
    Track et = track;
    connect(editBtn, &QPushButton::clicked, [this, et]() { emit editTrackRequested(et); });
    layout->addWidget(editBtn);

    // Delete button
    auto *delBtn = new QPushButton(QStringLiteral("\uE107"));
    delBtn->setFixedSize(28, 28);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setFont(Theme::iconFont(11));
    delBtn->setToolTip(Lang::tr("Excluir música"));
    lumen::design::StyleSheet::apply(delBtn, QString(
        "QPushButton { background: transparent; color: %1; border: none; font-family: \"Segoe MDL2 Assets\"; }"
        "QPushButton:hover { color: %2; }"
    ).arg(Theme::textMuted().name(), Theme::danger().name()));
    int delId = track.id;
    connect(delBtn, &QPushButton::clicked, [this, delId]() { emit deleteRequested(delId); });
    layout->addWidget(delBtn);

    // Duration
    auto *dur = new QLabel(Theme::formatTime(track.durationMs));
    dur->setFont(Theme::monoFont(11));
    dur->setFixedWidth(40);
    dur->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lumen::design::StyleSheet::apply(dur, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    layout->addWidget(dur);

    // Click to play via transparent overlay button
    Track t = track;
    auto *overlay = new QPushButton(row);
    overlay->setGeometry(0, 0, 9999, 52);
    lumen::design::StyleSheet::apply(overlay, "background: transparent; border: none;");
    overlay->setCursor(Qt::PointingHandCursor);
    overlay->lower();
    connect(overlay, &QPushButton::clicked, [this, t]() { emit playRequested(t); });
    // Hover over the row's main area swaps the index number for a play glyph.
    if (!active) overlay->installEventFilter(new HoverPlayFilter(idx, idxNum, overlay));

    return row;
}

QWidget *HomePage::createFolderChip(const Folder &folder, int trackCount) {
    auto *chip = new QPushButton();
    chip->setFixedHeight(64);
    chip->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(chip, QString(
        "QPushButton { background: %1; border: none; border-radius: 8px; }"
        "QPushButton:hover { background: %2; }"
    ).arg(Theme::card().name(), Theme::cardHover().name()));

    auto *layout = new QHBoxLayout(chip);
    layout->setContentsMargins(8, 8, 12, 8);
    layout->setSpacing(10);

    // Cover: image (if set) > mosaic (4+ tracks) > folder gradient — same
    // hierarchy as the cards on the Playlists page.
    layout->addWidget(createChipCover(folder), 0, Qt::AlignVCenter);

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

QWidget *HomePage::createChipCover(const Folder &folder) {
    return CoverWidget::playlistCover(folder, m_model->tracksInFolder(folder.name), 48, 6);
}

QWidget *HomePage::createRecentCard(const Folder &folder) {
    auto tracks = m_model->tracksInFolder(folder.name);

    auto *card = new QPushButton();
    card->setFixedSize(150, 198);
    card->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(card, QString(
        "QPushButton { background: %1; border: none; border-radius: 10px; }"
        "QPushButton:hover { background: %2; }"
    ).arg(Theme::card().name(), Theme::cardHover().name()));

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 10);
    layout->setSpacing(6);

    layout->addWidget(CoverWidget::playlistCover(folder, tracks, 126, 8), 0, Qt::AlignHCenter);

    auto *nameLabel = new QLabel(folder.name);
    nameLabel->setFont(Theme::bodyFont(12));
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    lumen::design::StyleSheet::apply(nameLabel, QString("color: %1; background: transparent; font-weight: bold;").arg(Theme::text().name()));
    layout->addWidget(nameLabel);

    auto *countLabel = new QLabel(QString(Lang::tr("%1 faixa%2"))
        .arg(tracks.size()).arg(tracks.size() != 1 ? "s" : ""));
    countLabel->setFont(Theme::bodyFont(10));
    countLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    lumen::design::StyleSheet::apply(countLabel, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    layout->addWidget(countLabel);

    QString name = folder.name;
    connect(card, &QPushButton::clicked, [this, name]() { emit navigateTo("folder", name); });
    return card;
}
