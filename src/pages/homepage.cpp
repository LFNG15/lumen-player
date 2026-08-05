#include "design/stylesheet.h"
#include "design/thememanager.h"
#include "homepage.h"
#include "lang.h"
#include "models/trackcontextmenu.h"
#include "models/trackrowdelegate.h"
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QPainter>
#include <QLinearGradient>
#include <QDateTime>
#include <QFrame>
#include <QCursor>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QPixmap>
#include <QListView>
#include <QShortcut>
#include <QWheelEvent>
#include "coverwidget.h"

// A shelf QListView never scrolls itself — wheel events pass through to the
// page's outer QScrollArea instead of getting stuck in a (possibly slightly
// mis-sized, see updateShelfHeight()) internal scroll range.
namespace {
class NonScrollingListView : public QListView {
public:
    using QListView::QListView;
protected:
    void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};
} // namespace

HomePage::HomePage(TrackModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

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

    // ── Top region: greeting/chips/"Recentes" strip + 2 capped shelves ──────
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    lumen::design::StyleSheet::apply(m_scroll, "QScrollArea { background: transparent; border: none; }");

    auto *content = new QWidget();
    lumen::design::StyleSheet::apply(content, "background: transparent;");
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(32, 28, 32, 28);
    contentLayout->setSpacing(12);

    // Dynamic sub-region: Folder-shaped content (greeting, chips, recents
    // cards) is unaffected by this migration and keeps its old
    // tear-down-and-rebuild-every-refresh() behavior, scoped to its own layout
    // instead of the whole page.
    m_dynamicRegion = new QWidget(content);
    lumen::design::StyleSheet::apply(m_dynamicRegion, "background: transparent;");
    m_dynamicLayout = new QVBoxLayout(m_dynamicRegion);
    m_dynamicLayout->setContentsMargins(0, 0, 0, 0);
    m_dynamicLayout->setSpacing(12);
    contentLayout->addWidget(m_dynamicRegion);

    m_playedSection = buildShelf(Lang::tr("Tocadas recentemente"),
        TrackListModel::Source::RecentlyPlayed, 8, /*allowDelete=*/false,
        &m_playedView, &m_playedModel);
    contentLayout->addWidget(m_playedSection);

    m_addedSection = buildShelf(Lang::tr("Adicionadas recentemente"),
        TrackListModel::Source::RecentlyAdded, 8, /*allowDelete=*/false,
        &m_addedView, &m_addedModel);
    contentLayout->addWidget(m_addedSection);
    contentLayout->addStretch();

    m_scroll->setWidget(content);
    outerLayout->addWidget(m_scroll, 1);

    // ── Bottom region: "Biblioteca Completa" — unbounded, so it keeps its own
    // independent scroll instead of nesting inside m_scroll. This project
    // already tried nested scroll areas for the other list pages and rejected
    // them (double scrollbar) — same reasoning applies here.
    m_librarySection = buildShelf(Lang::tr("Biblioteca Completa"),
        TrackListModel::Source::All, 0, /*allowDelete=*/true,
        &m_libraryView, &m_libraryModel);
    outerLayout->addWidget(m_librarySection, 1);
}

int HomePage::chipColumnsForWidth(int w) const
{
    return qBound(1, (qMax(240, w - 64) + 8) / 208, 4);
}

void HomePage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const int cols = chipColumnsForWidth(width());
    if (cols != m_lastChipCols && m_lastChipCols >= 0) {
        m_lastChipCols = cols;
        refresh(m_lastCurrentId, m_lastPlaying);
    } else if (m_lastChipCols < 0) {
        m_lastChipCols = cols;
    }
}

QWidget *HomePage::buildShelf(const QString &labelText, TrackListModel::Source::Kind kind,
                              int limit, bool allowDelete,
                              QListView **outView, TrackListModel **outModel)
{
    auto *section = new QWidget();
    lumen::design::StyleSheet::apply(section, "background: transparent;");
    auto *sectionLayout = new QVBoxLayout(section);
    sectionLayout->setContentsMargins(0, 0, 0, 0);
    sectionLayout->setSpacing(4);

    auto *label = new QLabel(labelText, section);
    label->setFont(Theme::titleFont(18));
    lumen::design::StyleSheet::apply(label, QString(
        "color: %1; background: transparent; padding-top: 8px;").arg(Theme::text().name()));
    sectionLayout->addWidget(label);

    auto *model = new TrackListModel(m_model, section);
    TrackListModel::Source src;
    src.kind = kind;
    src.limit = limit;
    model->setSource(src);
    model->setReorderEnabled(false);

    auto *view = limit > 0 ? new NonScrollingListView(section) : new QListView(section);
    view->setModel(model);
    view->setUniformItemSizes(true);
    view->setLayoutMode(QListView::Batched);
    view->setResizeMode(QListView::Adjust);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setMouseTracking(true);
    view->setFrameShape(QFrame::NoFrame);
    view->setSpacing(2);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    lumen::design::StyleSheet::apply(view, QStringLiteral(
        "QListView { background: transparent; border: none; outline: none; }"));
    if (limit > 0) {
        // Capped shelf: height follows content (see updateShelfHeight()), no
        // internal scrollbar, and it shouldn't steal keyboard focus from the
        // page just for being present.
        view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setFocusPolicy(Qt::NoFocus);
    }

    auto *delegate = new TrackRowDelegate(view);
    view->setItemDelegate(delegate);

    connect(delegate, &TrackRowDelegate::playClicked, this, [this, model](const QModelIndex &idx) {
        const int id = model->trackIdAt(idx.row());
        if (Track *t = m_model->findTrack(id)) emit playRequested(*t);
    });
    connect(delegate, &TrackRowDelegate::likeClicked, this, [this, model](const QModelIndex &idx) {
        const int id = model->trackIdAt(idx.row());
        if (id > 0) emit likeToggled(id);
    });
    connect(delegate, &TrackRowDelegate::moreClicked, this,
            [this, model, view](const QModelIndex &idx, const QPoint &gp) {
        QList<int> ids = selectedIds(view, model);
        if (ids.isEmpty()) {
            const int id = model->trackIdAt(idx.row());
            if (id > 0) ids.append(id);
        }
        if (!ids.isEmpty())
            m_ctx->popupAddMenu(ids, gp);
    });
    connect(view, &QListView::customContextMenuRequested, this, [this, model, view](const QPoint &pos) {
        const auto ids = selectedIds(view, model);
        if (!ids.isEmpty())
            m_ctx->popup(ids, view->viewport()->mapToGlobal(pos));
    });

    // Keyboard shortcuts only on the unbounded (library) list. All three lists
    // are visible on Home at once — unlike FolderDetailPage/LikedPage/SearchPage,
    // where only one is ever visible, so Qt's default WindowShortcut context
    // never has to disambiguate between them. Registering the same Space/L/Q/Menu
    // shortcuts on all three here would make them ambiguous whenever no view has
    // focus. The two capped shelves already can't take focus (NoFocus above) —
    // they're mouse/click only, consistent with being a preview, not a primary list.
    if (limit == 0) {
        auto *spaceKey = new QShortcut(Qt::Key_Space, view);
        connect(spaceKey, &QShortcut::activated, this, [this, model, view]() {
            const auto ids = selectedIds(view, model);
            if (ids.isEmpty()) return;
            if (Track *t = m_model->findTrack(ids.first())) emit playRequested(*t);
        });
        auto *likeKey = new QShortcut(Qt::Key_L, view);
        connect(likeKey, &QShortcut::activated, this, [this, model, view]() {
            for (int id : selectedIds(view, model)) emit likeToggled(id);
        });
        auto *qKey = new QShortcut(Qt::Key_Q, view);
        connect(qKey, &QShortcut::activated, this, [this, model, view]() {
            for (int id : selectedIds(view, model))
                if (Track *t = m_model->findTrack(id)) emit enqueueRequested(*t);
        });
        auto *menuKey = new QShortcut(Qt::Key_Menu, view);
        connect(menuKey, &QShortcut::activated, this, [this, model, view]() {
            const auto ids = selectedIds(view, model);
            if (!ids.isEmpty()) m_ctx->popup(ids, QCursor::pos());
        });
        if (allowDelete) {
            auto *delKey = new QShortcut(QKeySequence::Delete, view);
            connect(delKey, &QShortcut::activated, this, [this, model, view]() {
                for (int id : selectedIds(view, model)) emit deleteRequested(id);
            });
        }
    }

    sectionLayout->addWidget(view);

    if (outView) *outView = view;
    if (outModel) *outModel = model;
    return section;
}

void HomePage::updateShelfHeight(QListView *view, TrackListModel *model)
{
    const int rows = model->rowCount();
    if (rows <= 0) {
        view->setFixedHeight(0);
        return;
    }
    const int rh = lumen::design::ThemeManager::m().rowHeight;
    // Estimate: no existing precedent in this codebase for a content-sized,
    // non-scrolling QListView — visually confirm against live rowHeight/
    // density changes rather than trusting this formula to be pixel-exact.
    view->setFixedHeight(rows * rh + (rows - 1) * view->spacing());
}

QList<int> HomePage::selectedIds(QListView *view, TrackListModel *model) const
{
    QList<int> ids;
    for (const QModelIndex &idx : view->selectionModel()->selectedRows()) {
        const int id = model->trackIdAt(idx.row());
        if (id > 0) ids.append(id);
    }
    if (ids.isEmpty() && view->currentIndex().isValid()) {
        const int id = model->trackIdAt(view->currentIndex().row());
        if (id > 0) ids.append(id);
    }
    return ids;
}

void HomePage::refresh(int currentTrackId, bool isPlaying) {
    m_lastCurrentId = currentTrackId;
    m_lastPlaying = isPlaying;
    m_lastChipCols = chipColumnsForWidth(width());

    // Clear only the dynamic region (greeting/chips/recents) — the three track
    // lists below reload() their existing models instead of being torn down.
    QLayoutItem *item;
    while ((item = m_dynamicLayout->takeAt(0)) != nullptr) {
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
    m_dynamicLayout->addWidget(greetLabel);

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
        ).arg(Theme::accent().name(), Theme::onAccent().name(), Theme::accentHover().name()));
        connect(addBtn, &QPushButton::clicked, [this]() { emit navigateTo("add"); });
        emptyLayout->addWidget(addBtn, 0, Qt::AlignCenter);

        emptyWidget->setMinimumHeight(350);
        m_dynamicLayout->addWidget(emptyWidget);
        m_playedSection->hide();
        m_addedSection->hide();
        m_librarySection->hide();
        return;
    }
    m_playedSection->show();
    m_addedSection->show();
    m_librarySection->show();

    // ── Folder chips ────────────────────────────────────────
    auto folders = m_model->folders();
    if (!folders.isEmpty()) {
        auto *grid = new QGridLayout();
        grid->setSpacing(8);
        const int chipCols = m_lastChipCols > 0 ? m_lastChipCols : chipColumnsForWidth(width());
        // Column stretch forces every column to share the grid's real width
        // equally, so the layout can never end up wider than the window no
        // matter how long an individual playlist name is (the chip itself
        // elides to fit — see createFolderChip).
        for (int c = 0; c < chipCols; ++c)
            grid->setColumnStretch(c, 1);
        const int chipW = qMax(120, (qMax(240, width() - 64) - (chipCols - 1) * 8) / chipCols);
        int col = 0, row = 0;
        for (auto &f : folders) {
            int count = m_model->tracksInFolder(f.name).size();
            auto *chip = createFolderChip(f, count, chipW);
            grid->addWidget(chip, row, col);
            col++;
            if (col >= chipCols) { col = 0; row++; }
        }
        // Liked chip — same layout as playlist chips, with a heart cover.
        auto liked = m_model->likedTracks();
        if (!liked.isEmpty()) {
            auto *likedChip = new QPushButton();
            likedChip->setFixedHeight(64);
            likedChip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            likedChip->setCursor(Qt::PointingHandCursor);
            lumen::design::StyleSheet::apply(likedChip, QString(
                "QPushButton { background: %1; border: none; border-radius: 8px; }"
                "QPushButton:hover { background: %2; }"
            ).arg(Theme::card().name(), Theme::cardHover().name()));

            auto *likedLayout = new QHBoxLayout(likedChip);
            likedLayout->setContentsMargins(8, 8, 12, 8);
            likedLayout->setSpacing(10);

            auto *heartCover = new QLabel("");
            heartCover->setFixedSize(48, 48);
            heartCover->setAttribute(Qt::WA_TransparentForMouseEvents);
            lumen::design::StyleSheet::apply(heartCover, QString(
                "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 6px;")
                .arg(Theme::accent().name(), Theme::accentDim().darker(160).name()));

            // Paint the glyph centered on its actual ink bounds instead of via
            // QLabel's Qt::AlignCenter, which centers Segoe MDL2 Assets glyphs by
            // their (uneven) advance box and leaves them drifting toward the
            // top-left — visible at this icon size.
            {
                const QString glyph = heartCover->text();
                const QFont font = Theme::iconFont(18);
                const QFontMetrics fm(font);
                const QRect ink = fm.tightBoundingRect(glyph);

                QPixmap pix(heartCover->size());
                pix.fill(Qt::transparent);
                QPainter p(&pix);
                p.setRenderHint(QPainter::Antialiasing);
                p.setFont(font);
                p.setPen(Theme::text());
                const int x = (pix.width()  - ink.width())  / 2 - ink.left();
                const int y = (pix.height() - ink.height()) / 2 - ink.top();
                p.drawText(x, y, glyph);
                heartCover->setPixmap(pix);
            }
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
        m_dynamicLayout->addWidget(gridWidget);
    }

    // ── Recents: last played playlists (Spotify-style cards) ─
    auto recentFolders = m_model->recentlyPlayedFolders(6);
    if (!recentFolders.isEmpty()) {
        auto *recentsLabel = new QLabel(Lang::tr("Recentes"));
        recentsLabel->setFont(Theme::titleFont(18));
        lumen::design::StyleSheet::apply(recentsLabel, QString("color: %1; background: transparent; padding-top: 8px;").arg(Theme::text().name()));
        m_dynamicLayout->addWidget(recentsLabel);

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
        m_dynamicLayout->addWidget(strip);
    }

    // ── Track lists: reload the persistent models, no widget rebuild ────────
    m_playedModel->reload();
    m_playedModel->setPlaybackState(currentTrackId, isPlaying);
    updateShelfHeight(m_playedView, m_playedModel);
    m_playedSection->setVisible(m_playedModel->rowCount() > 0);

    m_addedModel->reload();
    m_addedModel->setPlaybackState(currentTrackId, isPlaying);
    updateShelfHeight(m_addedView, m_addedModel);
    m_addedSection->setVisible(m_addedModel->rowCount() > 0);

    m_libraryModel->reload();
    m_libraryModel->setPlaybackState(currentTrackId, isPlaying);
}

QWidget *HomePage::createFolderChip(const Folder &folder, int trackCount, int chipWidth) {
    auto *chip = new QPushButton();
    chip->setFixedHeight(64);
    // Grow to fill its (stretched) grid column instead of just its natural
    // content width — paired with the column stretch in refresh(), this is
    // what actually keeps the grid's real width in sync with the column
    // count chipColumnsForWidth() chose, instead of drifting wider than the
    // window as soon as a playlist name is long.
    chip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
    auto *nameLabel = new QLabel();
    nameLabel->setFont(Theme::bodyFont(13));
    nameLabel->setMinimumWidth(0);
    nameLabel->setToolTip(folder.name);
    const int textAvail = qMax(40, chipWidth - 20 /*chip margins*/ - 48 /*cover*/ - 10 /*spacing*/);
    nameLabel->setText(QFontMetrics(nameLabel->font()).elidedText(folder.name, Qt::ElideRight, textAvail));
    lumen::design::StyleSheet::apply(nameLabel, QString("color: %1; background: transparent; font-weight: bold;").arg(Theme::text().name()));
    auto *countLabel = new QLabel(QString(Lang::tr("%1 faixa%2")).arg(trackCount).arg(trackCount != 1 ? "s" : ""));
    countLabel->setFont(Theme::bodyFont(10));
    countLabel->setMinimumWidth(0);
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
