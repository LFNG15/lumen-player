#include "design/stylesheet.h"
#include "folderdetailpage.h"
#include "database.h"
#include "lang.h"
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QDialog>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QMenu>
#include <QGridLayout>
#include <QListWidget>
#include <QSettings>
#include <algorithm>
#include "hoverplayfilter.h"
#include "reorderablelist.h"
#include "textutils.h"

FolderDetailPage::FolderDetailPage(TrackModel *model, QWidget *parent)
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
    m_contentLayout->setSpacing(8);

    scroll->setWidget(content);
    outerLayout->addWidget(scroll);
}

// Sort modes selectable per playlist ("custom" keeps the user's drag order).
struct SortMode { const char *id; const char *label; };
static const SortMode kSortModes[] = {
    {"custom",   "Personalizada"},
    {"title",    "Título"},
    {"artist",   "Artista"},
    {"recent",   "Adicionadas recentemente"},
    {"oldest",   "Mais antigas"},
    {"duration", "Duração"},
};

static void applySortMode(QList<Track> &tracks, const QString &mode) {
    auto title  = [](const Track &t) { return TextUtils::normalized(t.title); };
    auto artist = [](const Track &t) { return TextUtils::normalized(t.artist); };

    if (mode == "title") {
        std::sort(tracks.begin(), tracks.end(),
            [&](const Track &a, const Track &b) { return title(a) < title(b); });
    } else if (mode == "artist") {
        std::sort(tracks.begin(), tracks.end(),
            [&](const Track &a, const Track &b) {
                return artist(a) != artist(b) ? artist(a) < artist(b)
                                              : title(a) < title(b);
            });
    } else if (mode == "recent") {
        std::sort(tracks.begin(), tracks.end(),
            [](const Track &a, const Track &b) {
                return a.addedAt != b.addedAt ? a.addedAt > b.addedAt : a.id > b.id;
            });
    } else if (mode == "oldest") {
        std::sort(tracks.begin(), tracks.end(),
            [](const Track &a, const Track &b) {
                return a.addedAt != b.addedAt ? a.addedAt < b.addedAt : a.id < b.id;
            });
    } else if (mode == "duration") {
        std::sort(tracks.begin(), tracks.end(),
            [&](const Track &a, const Track &b) {
                return a.durationMs != b.durationMs ? a.durationMs < b.durationMs
                                                    : title(a) < title(b);
            });
    }
    // "custom" (default): keep the position-based order from the model.
}

QString FolderDetailPage::sortMode() const {
    if (m_folderId > 0)
        return Database::instance().playlistSortMode(m_folderId);
    return QSettings().value(QString("playlistSort/%1").arg(m_folderId), "custom").toString();
}

void FolderDetailPage::setFolder(const QString &folderName) {
    m_folderName = folderName;
    m_filterText.clear();   // each playlist starts with an empty search
    // Look up folder ID
    m_folderId = 0;
    if (!folderName.isEmpty()) {
        for (auto &f : m_model->folders()) {
            if (f.name == folderName) {
                m_folderId = f.id;
                break;
            }
        }
    }
}

void FolderDetailPage::refresh(int currentTrackId, bool isPlaying) {
    QLayoutItem *item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    bool isStandalone = m_folderName.isEmpty();

    // Back button
    auto *backBtn = new QPushButton("\uE0A6");
    backBtn->setFixedSize(34, 34);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFont(Theme::iconFont(12));
    lumen::design::StyleSheet::apply(backBtn, QString(
        "QPushButton { background: rgba(255,255,255,0.05); color: %1; border: none; border-radius: 17px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }"
    ).arg(Theme::text().name()));
    connect(backBtn, &QPushButton::clicked, this, &FolderDetailPage::navigateBack);
    m_contentLayout->addWidget(backBtn, 0, Qt::AlignLeft);

    auto tracks = isStandalone ? m_model->standaloneTracks() : m_model->tracksInFolder(m_folderName);
    applySortMode(tracks, sortMode());
    m_displayedTracks = tracks;
    m_lastCurrentId = currentTrackId;
    m_lastPlaying   = isPlaying;

    qint64 total = 0;
    for (auto &t : tracks) total += t.durationMs;

    // Header
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(20);

    auto *cover = new QWidget();
    cover->setFixedSize(140, 140);
    if (isStandalone) {
        if (tracks.size() >= 4) {
            auto *coverGrid = new QGridLayout(cover);
            coverGrid->setSpacing(2);
            coverGrid->setContentsMargins(0, 0, 0, 0);
            for (int i = 0; i < 4; ++i) {
                auto *cell = new QWidget();
                lumen::design::StyleSheet::apply(cell, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 4px;")
                    .arg(tracks[i].cover.c1.name(), tracks[i].cover.c2.name()));
                coverGrid->addWidget(cell, i / 2, i % 2);
            }
        } else {
            auto g = tracks.isEmpty() ? Theme::randomPalette() : tracks[0].cover;
            lumen::design::StyleSheet::apply(cover, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 10px;")
                .arg(g.c1.name(), g.c2.name()));
        }
    } else {
        // Cover image (if set) > mosaic (4+ tracks) > folder gradient
        Theme::GradientPair folderCover;
        QString folderImage;
        for (auto &f : m_model->folders()) {
            if (f.name == m_folderName) { folderCover = f.cover; folderImage = f.coverImage; break; }
        }
        QPixmap coverPix = folderImage.isEmpty() ? QPixmap()
                                                 : Theme::roundedCover(folderImage, 140, 140, 10);
        if (!coverPix.isNull()) {
            auto *imgLabel = new QLabel(cover);
            imgLabel->setGeometry(0, 0, 140, 140);
            imgLabel->setPixmap(coverPix);
            lumen::design::StyleSheet::apply(imgLabel, "background: transparent;");

            // Click the cover image to view it enlarged (like Spotify).
            cover->setCursor(Qt::PointingHandCursor);
            cover->setToolTip(Lang::tr("Ver imagem"));
            auto *zoom = new QPushButton(cover);
            zoom->setGeometry(0, 0, 140, 140);
            lumen::design::StyleSheet::apply(zoom, "background: transparent; border: none;");
            zoom->setCursor(Qt::PointingHandCursor);
            QString img = folderImage;
            connect(zoom, &QPushButton::clicked, [this, img]() { showCoverLightbox(img); });
        } else if (tracks.size() >= 4) {
            auto *coverGrid = new QGridLayout(cover);
            coverGrid->setSpacing(2);
            coverGrid->setContentsMargins(0, 0, 0, 0);
            for (int i = 0; i < 4; ++i) {
                auto *cell = new QWidget();
                lumen::design::StyleSheet::apply(cell, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 4px;")
                    .arg(tracks[i].cover.c1.name(), tracks[i].cover.c2.name()));
                coverGrid->addWidget(cell, i / 2, i % 2);
            }
        } else {
            auto g = (folderCover.c1.isValid() && folderCover.c1 != QColor()) ? folderCover : (tracks.isEmpty() ? Theme::randomPalette() : tracks[0].cover);
            lumen::design::StyleSheet::apply(cover, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 10px;")
                .arg(g.c1.name(), g.c2.name()));
        }
    }
    headerLayout->addWidget(cover);

    auto *infoLayout = new QVBoxLayout();
    infoLayout->addStretch();
    auto *typeLabel = new QLabel(isStandalone ? Lang::tr("MÚSICAS AVULSAS") : Lang::tr("PLAYLIST"));
    typeLabel->setFont(Theme::bodyFont(10));
    lumen::design::StyleSheet::apply(typeLabel, QString("color: %1; background: transparent; font-weight: bold; letter-spacing: 1px;").arg(Theme::textMuted().name()));
    infoLayout->addWidget(typeLabel);

    auto *nameRow = new QHBoxLayout();
    auto *nameLabel = new QLabel(isStandalone ? Lang::tr("Músicas avulsas") : m_folderName);
    nameLabel->setFont(Theme::titleFont(28));
    lumen::design::StyleSheet::apply(nameLabel, QString("color: %1; background: transparent;").arg(Theme::text().name()));
    nameRow->addWidget(nameLabel);

    // Edit button (only for real playlists)
    if (!isStandalone) {
        auto *editBtn = new QPushButton("\uE70F");
        editBtn->setFixedSize(32, 32);
        editBtn->setCursor(Qt::PointingHandCursor);
        editBtn->setFont(Theme::iconFont(12));
        editBtn->setToolTip(Lang::tr("Editar playlist"));
        lumen::design::StyleSheet::apply(editBtn, QString(
            "QPushButton { background: rgba(255,255,255,0.05); color: %1; border: none; border-radius: 16px; font-family: \"Segoe MDL2 Assets\"; }"
            "QPushButton:hover { background: rgba(255,255,255,0.12); color: %2; }"
        ).arg(Theme::textMuted().name(), Theme::accent().name()));
        connect(editBtn, &QPushButton::clicked, this, &FolderDetailPage::showEditDialog);
        nameRow->addWidget(editBtn);

        // Delete button
        auto *deletePlaylistBtn = new QPushButton("\uE107");
        deletePlaylistBtn->setFixedSize(32, 32);
        deletePlaylistBtn->setCursor(Qt::PointingHandCursor);
        deletePlaylistBtn->setFont(Theme::iconFont(12));
        deletePlaylistBtn->setToolTip(Lang::tr("Excluir playlist"));
        lumen::design::StyleSheet::apply(deletePlaylistBtn, QString(
            "QPushButton { background: rgba(255,255,255,0.05); color: %1; border: none; border-radius: 16px; font-family: \"Segoe MDL2 Assets\"; }"
            "QPushButton:hover { background: rgba(255,255,255,0.12); color: %2; }"
        ).arg(Theme::textMuted().name(), Theme::danger().name()));
        int fid = m_folderId;
        QString fname = m_folderName;
        connect(deletePlaylistBtn, &QPushButton::clicked, [this, fid, fname]() {
            auto *dlg = new QMessageBox(this);
            dlg->setWindowTitle(Lang::tr("Excluir Playlist"));
            dlg->setText(QString(Lang::tr("Excluir a playlist \"%1\"?")).arg(fname));
            dlg->setInformativeText(Lang::tr("As músicas não serão apagadas — ficarão como músicas avulsas."));
            dlg->setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
            dlg->setDefaultButton(QMessageBox::Cancel);
            lumen::design::StyleSheet::apply(dlg, QString(
                "QMessageBox { background: %1; color: %2; } QLabel { color: %2; background: transparent; }"
                "QPushButton { background: %3; color: %2; border: 1px solid %4; border-radius: 8px; padding: 6px 16px; min-width: 70px; }"
            ).arg(Theme::surface().name(), Theme::text().name(), Theme::card().name(), Theme::border().name()));
            if (dlg->exec() == QMessageBox::Yes) {
                m_model->deletePlaylist(fid);
                emit navigateBack();
            }
        });
        nameRow->addWidget(deletePlaylistBtn);
    }
    nameRow->addStretch();
    infoLayout->addLayout(nameRow);

    auto *statsLabel = new QLabel(QString(Lang::tr("%1 faixa%2%3"))
        .arg(tracks.size())
        .arg(tracks.size() != 1 ? "s" : "")
        .arg(total > 0 ? QString(" • %1").arg(Theme::formatTime(total)) : ""));
    statsLabel->setFont(Theme::bodyFont(12));
    lumen::design::StyleSheet::apply(statsLabel, QString("color: %1; background: transparent;").arg(Theme::textSoft().name()));
    infoLayout->addWidget(statsLabel);
    infoLayout->addStretch();

    headerLayout->addLayout(infoLayout, 1);

    auto *headerWidget = new QWidget();
    headerWidget->setLayout(headerLayout);
    lumen::design::StyleSheet::apply(headerWidget, "background: transparent;");
    m_contentLayout->addWidget(headerWidget);
    m_contentLayout->addSpacing(12);

    // Controls row: play + in-playlist search + sort selector
    if (!tracks.isEmpty()) {
        auto *controlsRow = new QHBoxLayout();
        controlsRow->setSpacing(10);

        auto *playBtn = new QPushButton("\uE102");
        playBtn->setFixedSize(48, 48);
        playBtn->setCursor(Qt::PointingHandCursor);
        playBtn->setFont(Theme::iconFont(16));
        lumen::design::StyleSheet::apply(playBtn, QString(
            "QPushButton { background: %1; color: %2; border: none; border-radius: 24px; font-family: \"Segoe MDL2 Assets\"; font-size: 16px; }"
            "QPushButton:hover { background: %3; }"
        ).arg(Theme::accent().name(), Theme::bg().name(), Theme::accent().lighter(110).name()));
        Track first = tracks[0];
        connect(playBtn, &QPushButton::clicked, [this, first]() { emit playRequested(first); });
        controlsRow->addWidget(playBtn);
        controlsRow->addStretch();

        // Filters the rows live without rebuilding the page (keeps focus).
        auto *searchEdit = new QLineEdit();
        searchEdit->setPlaceholderText(Lang::tr("Buscar na playlist"));
        searchEdit->setText(m_filterText);
        searchEdit->setFont(Theme::bodyFont(11));
        searchEdit->setClearButtonEnabled(true);
        searchEdit->setFixedSize(220, 34);
        lumen::design::StyleSheet::apply(searchEdit, QString(
            "QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 17px; padding: 0 14px; }"
            "QLineEdit:focus { border-color: %4; }"
        ).arg(Theme::surface().name(), Theme::text().name(),
              Theme::border().name(), Theme::accent().name()));
        connect(searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
            m_filterText = text;
            applyFilter();
        });
        controlsRow->addWidget(searchEdit);

        QString sortLabel;
        for (const auto &m : kSortModes)
            if (sortMode() == m.id) { sortLabel = Lang::tr(m.label); break; }
        auto *sortBtn = new QPushButton(QString("  %1").arg(sortLabel));
        sortBtn->setFixedHeight(34);
        sortBtn->setCursor(Qt::PointingHandCursor);
        sortBtn->setFont(Theme::bodyFont(11));
        sortBtn->setToolTip(Lang::tr("Ordenar"));
        lumen::design::StyleSheet::apply(sortBtn, QString(
            "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 17px; padding: 0 14px; font-family: \"Segoe UI\", \"Segoe MDL2 Assets\"; }"
            "QPushButton:hover { color: %3; border-color: %3; }"
        ).arg(Theme::textSoft().name(), Theme::border().name(), Theme::accent().name()));
        connect(sortBtn, &QPushButton::clicked, this, &FolderDetailPage::showSortMenu);
        controlsRow->addWidget(sortBtn);

        auto *controlsWidget = new QWidget();
        controlsWidget->setLayout(controlsRow);
        lumen::design::StyleSheet::apply(controlsWidget, "background: transparent;");
        m_contentLayout->addWidget(controlsWidget);
        m_contentLayout->addSpacing(8);
    }

    // Track list — a QListWidget so rows can be reordered by drag-and-drop.
    auto *list = new ReorderableList();
    m_trackList = list;
    list->setFrameShape(QFrame::NoFrame);
    lumen::design::StyleSheet::apply(list, "QListWidget { background: transparent; border: none; }"
                        "QListWidget::item { border: none; }"
                        "QListWidget::item:selected { background: transparent; }");
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setSpacing(4);
    // Drag-to-reorder only makes sense inside a real playlist showing the
    // custom order (and with no search filter active — applyFilter() handles
    // that part dynamically).
    m_canReorder = !isStandalone && sortMode() == "custom";
    if (m_canReorder) {
        list->setDragDropMode(QAbstractItemView::InternalMove);
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setDefaultDropAction(Qt::MoveAction);
    } else {
        list->setSelectionMode(QAbstractItemView::NoSelection);
    }

    for (int i = 0; i < tracks.size(); ++i) {
        auto &track = tracks[i];
        bool active = (track.id == currentTrackId);

        auto *row = new QWidget();
        row->setObjectName("trackRow");
        row->setFixedHeight(52);
        // The row itself ignores mouse events (they propagate to the list,
        // which handles click-to-play and drag-to-reorder), but its buttons
        // must stay clickable — so no WA_TransparentForMouseEvents here.
        lumen::design::StyleSheet::apply(row, QString(
            "QWidget#trackRow { background: %1; border-radius: 8px; border-left: 3px solid %2; }"
        ).arg(active ? Theme::accentRgba(0.12) : QStringLiteral("transparent"),
              active ? Theme::accent().name() : "transparent"));

        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(12, 4, 12, 4);
        layout->setSpacing(12);

        QString idxNum = QString("%1").arg(i + 1, 2, 10, QChar('0'));
        auto *idx = new QLabel(active ? QStringLiteral("\uE102") : idxNum);
        idx->setFont(Theme::monoFont(12));
        idx->setFixedWidth(28);
        idx->setAlignment(Qt::AlignCenter);
        lumen::design::StyleSheet::apply(idx, QString("color: %1; background: transparent; font-family: \"Segoe MDL2 Assets\", Consolas;").arg(active ? Theme::accent().name() : Theme::textMuted().name()));
        idx->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(idx);
        // Hovering the row swaps the number for a play glyph.
        if (!active) row->installEventFilter(new HoverPlayFilter(idx, idxNum, row));

        auto *swatch = new QWidget();
        swatch->setFixedSize(38, 38);
        swatch->setAttribute(Qt::WA_TransparentForMouseEvents);
        lumen::design::StyleSheet::apply(swatch, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 6px;")
            .arg(track.cover.c1.name(), track.cover.c2.name()));
        layout->addWidget(swatch);

        auto *infoCol = new QVBoxLayout();
        infoCol->setSpacing(1);
        auto *titleLabel = new QLabel(track.title);
        titleLabel->setFont(Theme::bodyFont(13));
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        lumen::design::StyleSheet::apply(titleLabel, QString("color: %1; background: transparent; font-weight: 600;").arg(active ? Theme::accent().name() : Theme::text().name()));
        auto *artistLabel = new QLabel(track.artist);
        artistLabel->setFont(Theme::bodyFont(11));
        artistLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        lumen::design::StyleSheet::apply(artistLabel, QString("color: %1; background: transparent;").arg(Theme::textSoft().name()));
        infoCol->addWidget(titleLabel);
        infoCol->addWidget(artistLabel);
        layout->addLayout(infoCol, 1);

        // Add to queue button
        auto *enqueueBtn = new QPushButton(QStringLiteral("\uE710"));
        enqueueBtn->setFixedSize(28, 28);
        enqueueBtn->setCursor(Qt::PointingHandCursor);
        enqueueBtn->setFont(Theme::iconFont(11));
        enqueueBtn->setToolTip(Lang::tr("Adicionar à fila"));
        lumen::design::StyleSheet::apply(enqueueBtn, QString(
            "QPushButton { background: transparent; color: %1; border: none; font-family: \"Segoe MDL2 Assets\"; }"
            "QPushButton:hover { color: %2; }"
        ).arg(Theme::textMuted().name(), Theme::accent().name()));
        Track eqt = track;
        connect(enqueueBtn, &QPushButton::clicked, [this, eqt]() { emit enqueueRequested(eqt); });
        layout->addWidget(enqueueBtn);

        // Move to playlist button
        auto *moveBtn = new QPushButton("\uE188");
        moveBtn->setFixedSize(28, 28);
        moveBtn->setCursor(Qt::PointingHandCursor);
        moveBtn->setFont(Theme::iconFont(11));
        moveBtn->setToolTip(Lang::tr("Adicionar à playlist"));
        lumen::design::StyleSheet::apply(moveBtn, QString(
            "QPushButton { background: transparent; color: %1; border: none; font-family: \"Segoe MDL2 Assets\"; }"
            "QPushButton:hover { color: %2; }"
        ).arg(Theme::textMuted().name(), Theme::accent().name()));
        int mid = track.id;
        connect(moveBtn, &QPushButton::clicked, [this, mid]() { showMoveDialog(mid); });
        layout->addWidget(moveBtn);

        auto *likeBtn = new QPushButton(track.liked ? "\uE00B" : "\uE006");
        likeBtn->setFixedSize(28, 28);
        likeBtn->setCursor(Qt::PointingHandCursor);
        lumen::design::StyleSheet::apply(likeBtn, QString("QPushButton { background: transparent; color: %1; border: none; font-size: 14px; font-family: \"Segoe MDL2 Assets\"; }").arg(
            track.liked ? Theme::accent().name() : Theme::textMuted().name()));
        int lid = track.id;
        connect(likeBtn, &QPushButton::clicked, [this, lid]() { emit likeToggled(lid); });
        layout->addWidget(likeBtn);

        auto *dur = new QLabel(Theme::formatTime(track.durationMs));
        dur->setFont(Theme::monoFont(11));
        dur->setFixedWidth(40);
        dur->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        dur->setAttribute(Qt::WA_TransparentForMouseEvents);
        lumen::design::StyleSheet::apply(dur, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
        layout->addWidget(dur);

        auto *editBtn = new QPushButton(QStringLiteral("\uE70F"));
        editBtn->setFixedSize(24, 24);
        editBtn->setCursor(Qt::PointingHandCursor);
        editBtn->setToolTip(Lang::tr("Editar m\u00FAsica"));
        lumen::design::StyleSheet::apply(editBtn, QString("QPushButton { background: transparent; color: %1; border: none; font-size: 11px; font-family: \"Segoe MDL2 Assets\"; } QPushButton:hover { color: %2; }").arg(
            Theme::textMuted().name(), Theme::accent().name()));
        Track et = track;
        connect(editBtn, &QPushButton::clicked, [this, et]() { emit editTrackRequested(et); });
        layout->addWidget(editBtn);

        auto *delBtn = new QPushButton("\uE107");
        delBtn->setFixedSize(24, 24);
        delBtn->setCursor(Qt::PointingHandCursor);
        lumen::design::StyleSheet::apply(delBtn, QString("QPushButton { background: transparent; color: %1; border: none; font-size: 11px; font-family: \"Segoe MDL2 Assets\"; } QPushButton:hover { color: %2; }").arg(
            Theme::textMuted().name(), Theme::danger().name()));
        int did = track.id;
        connect(delBtn, &QPushButton::clicked, [this, did]() { emit deleteRequested(did); });
        layout->addWidget(delBtn);

        auto *listItem = new QListWidgetItem(list);
        listItem->setSizeHint(QSize(0, 52));
        listItem->setData(Qt::UserRole, track.id);
        // Pre-normalized haystack for the in-playlist search.
        listItem->setData(Qt::UserRole + 1,
                          TextUtils::normalized(track.title + " " + track.artist));
        list->setItemWidget(listItem, row);
    }

    // Fit the list inside the page's scroll area (no inner scrollbar).
    list->setFixedHeight(tracks.size() * 60 + 12);

    connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        int id = it->data(Qt::UserRole).toInt();
        if (Track *t = m_model->findTrack(id)) emit playRequested(*t);
    });
    connect(list, &ReorderableList::moveRequested, this, [this](int from, int to) {
        QList<int> ids;
        for (int i = 0; i < m_trackList->count(); ++i)
            ids << m_trackList->item(i)->data(Qt::UserRole).toInt();
        if (from < 0 || from >= ids.size()) return;
        ids.move(from, to);
        m_model->reorderPlaylist(m_folderName, ids);
    });

    m_contentLayout->addWidget(list);
    m_contentLayout->addStretch();

    // Re-apply a search that was active before this rebuild.
    applyFilter();
}

void FolderDetailPage::applyFilter() {
    if (!m_trackList) return;
    const QString needle = TextUtils::normalized(m_filterText);
    int visible = 0;
    for (int i = 0; i < m_trackList->count(); ++i) {
        auto *it = m_trackList->item(i);
        const bool show = needle.isEmpty()
            || it->data(Qt::UserRole + 1).toString().contains(needle);
        it->setHidden(!show);
        if (show) ++visible;
    }
    m_trackList->setFixedHeight(visible * 60 + 12);
    // Hidden rows would corrupt drag indices, so reordering pauses while a
    // search is active.
    m_trackList->setDragDropMode(m_canReorder && needle.isEmpty()
        ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);
}

void FolderDetailPage::showSortMenu() {
    auto *menu = new QMenu(this);
    lumen::design::StyleSheet::apply(menu, QString(
        "QMenu { background: %1; border: 1px solid %2; border-radius: 8px; padding: 4px; color: %3; }"
        "QMenu::item { padding: 8px 16px; border-radius: 4px; }"
        "QMenu::item:selected { background: %4; }"
    ).arg(Theme::card().name(), Theme::border().name(), Theme::text().name(), Theme::cardHover().name()));

    const QString current = sortMode();
    for (const auto &m : kSortModes) {
        QString label = Lang::tr(m.label);
        if (current == m.id) label = "✓ " + label;
        QString id = m.id;
        menu->addAction(label, [this, id]() {
            if (m_folderId > 0)
                Database::instance().setPlaylistSortMode(m_folderId, id);
            else
                QSettings().setValue(QString("playlistSort/%1").arg(m_folderId), id);
            refresh(m_lastCurrentId, m_lastPlaying);
        });
    }
    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void FolderDetailPage::showEditDialog() {
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(Lang::tr("Editar Playlist"));
    dlg->setFixedSize(380, 300);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    lumen::design::StyleSheet::apply(dlg, QString(
        "QDialog { background: %1; }"
        "QLabel { background: transparent; color: %2; }"
        "QLineEdit { background: %3; color: %2; border: 1px solid %4; border-radius: 8px; padding: 8px 12px; }"
        "QLineEdit:focus { border-color: %5; }"
    ).arg(Theme::surface().name(), Theme::text().name(), Theme::bg().name(),
          Theme::border().name(), Theme::accent().name()));

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *nameLabel = new QLabel(Lang::tr("Nome da playlist"));
    nameLabel->setFont(Theme::bodyFont(12));
    layout->addWidget(nameLabel);

    auto *nameEdit = new QLineEdit(m_folderName);
    nameEdit->setFont(Theme::bodyFont(13));
    nameEdit->selectAll();
    layout->addWidget(nameEdit);

    // Get current cover
    Theme::GradientPair currentCover;
    QString currentImage;
    for (auto &f : m_model->folders()) {
        if (f.name == m_folderName) { currentCover = f.cover; currentImage = f.coverImage; break; }
    }

    auto *colorLabel = new QLabel(Lang::tr("Cores da capa:"));
    colorLabel->setFont(Theme::bodyFont(12));
    layout->addWidget(colorLabel);

    QColor *c1 = new QColor(currentCover.c1.isValid() ? currentCover.c1 : Theme::accent());
    QColor *c2 = new QColor(currentCover.c2.isValid() ? currentCover.c2 : Theme::danger());

    auto *colorRow = new QHBoxLayout();
    colorRow->setSpacing(8);

    auto *preview = new QWidget();
    preview->setFixedSize(50, 32);
    lumen::design::StyleSheet::apply(preview, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 6px;")
        .arg(c1->name(), c2->name()));
    colorRow->addWidget(preview);

    auto updatePrev = [preview, c1, c2]() {
        lumen::design::StyleSheet::apply(preview, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 6px;")
            .arg(c1->name(), c2->name()));
    };

    auto *btn1 = new QPushButton(Lang::tr("Cor 1"));
    btn1->setFixedSize(60, 32);
    btn1->setCursor(Qt::PointingHandCursor);
    btn1->setFont(Theme::bodyFont(11));
    lumen::design::StyleSheet::apply(btn1, QString("background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px; color: white;").arg(c1->name()));
    connect(btn1, &QPushButton::clicked, [btn1, c1, updatePrev, dlg]() {
        QColor chosen = QColorDialog::getColor(*c1, dlg, Lang::tr("Cor 1"));
        if (chosen.isValid()) {
            *c1 = chosen;
            lumen::design::StyleSheet::apply(btn1, QString("background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px; color: white;").arg(c1->name()));
            updatePrev();
        }
    });
    colorRow->addWidget(btn1);

    auto *btn2 = new QPushButton(Lang::tr("Cor 2"));
    btn2->setFixedSize(60, 32);
    btn2->setCursor(Qt::PointingHandCursor);
    btn2->setFont(Theme::bodyFont(11));
    lumen::design::StyleSheet::apply(btn2, QString("background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px; color: white;").arg(c2->name()));
    connect(btn2, &QPushButton::clicked, [btn2, c2, updatePrev, dlg]() {
        QColor chosen = QColorDialog::getColor(*c2, dlg, Lang::tr("Cor 2"));
        if (chosen.isValid()) {
            *c2 = chosen;
            lumen::design::StyleSheet::apply(btn2, QString("background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px; color: white;").arg(c2->name()));
            updatePrev();
        }
    });
    colorRow->addWidget(btn2);
    colorRow->addStretch();
    layout->addLayout(colorRow);

    // Optional cover image (overrides the gradient when set)
    auto *imageLabel = new QLabel(Lang::tr("Imagem da capa:"));
    imageLabel->setFont(Theme::bodyFont(12));
    layout->addWidget(imageLabel);

    QString *imagePath = new QString(currentImage);
    auto *imageRow = new QHBoxLayout();
    imageRow->setSpacing(8);

    auto *imgPreview = new QLabel();
    imgPreview->setFixedSize(50, 32);
    lumen::design::StyleSheet::apply(imgPreview, QString("background: %1; border-radius: 6px;").arg(Theme::bg().name()));
    if (!currentImage.isEmpty()) {
        QPixmap pm = Theme::roundedCover(currentImage, 50, 32, 6);
        if (!pm.isNull()) imgPreview->setPixmap(pm);
    }
    imageRow->addWidget(imgPreview);

    auto *pickImageBtn = new QPushButton(Lang::tr("Escolher imagem"));
    pickImageBtn->setFont(Theme::bodyFont(11));
    pickImageBtn->setFixedHeight(32);
    pickImageBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(pickImageBtn, QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 8px; padding: 0 10px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.05); }"
    ).arg(Theme::textSoft().name(), Theme::border().name()));
    connect(pickImageBtn, &QPushButton::clicked, [dlg, imagePath, imgPreview]() {
        QString file = QFileDialog::getOpenFileName(dlg, Lang::tr("Escolher imagem da capa"), QString(),
            Lang::tr("Imagens (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (file.isEmpty()) return;
        *imagePath = file;
        QPixmap pm = Theme::roundedCover(file, 50, 32, 6);
        if (!pm.isNull()) imgPreview->setPixmap(pm);
    });
    imageRow->addWidget(pickImageBtn);

    auto *clearImageBtn = new QPushButton(Lang::tr("Remover"));
    clearImageBtn->setFont(Theme::bodyFont(11));
    clearImageBtn->setFixedHeight(32);
    clearImageBtn->setCursor(Qt::PointingHandCursor);
    clearImageBtn->setToolTip(Lang::tr("Voltar a usar o gradiente de cores"));
    lumen::design::StyleSheet::apply(clearImageBtn, QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 8px; padding: 0 10px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.05); }"
    ).arg(Theme::textSoft().name(), Theme::border().name()));
    connect(clearImageBtn, &QPushButton::clicked, [imagePath, imgPreview]() {
        imagePath->clear();
        imgPreview->setPixmap(QPixmap());
        lumen::design::StyleSheet::apply(imgPreview, QString("background: %1; border-radius: 6px;").arg(Theme::bg().name()));
    });
    imageRow->addWidget(clearImageBtn);
    imageRow->addStretch();
    layout->addLayout(imageRow);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(Lang::tr("Cancelar"));
    cancelBtn->setFont(Theme::bodyFont(12));
    cancelBtn->setFixedHeight(36);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(cancelBtn, QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 18px; padding: 0 16px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.05); }"
    ).arg(Theme::textSoft().name(), Theme::border().name()));
    connect(cancelBtn, &QPushButton::clicked, [dlg, c1, c2, imagePath]() { delete c1; delete c2; delete imagePath; dlg->reject(); });
    btnRow->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton(Lang::tr("Salvar"));
    saveBtn->setFont(Theme::bodyFont(12));
    saveBtn->setFixedHeight(36);
    saveBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(saveBtn, QString(
        "QPushButton { background: %1; color: %2; border: none; border-radius: 18px; padding: 0 20px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }"
    ).arg(Theme::accent().name(), Theme::bg().name(), Theme::accent().lighter(110).name()));
    connect(saveBtn, &QPushButton::clicked, [this, dlg, nameEdit, c1, c2, imagePath, currentImage]() {
        QString newName = nameEdit->text().trimmed();
        if (newName.isEmpty()) return;
        m_model->updatePlaylistCover(m_folderId, *c1, *c2);
        // Only touch the image when it changed, to avoid re-copying the file.
        if (*imagePath != currentImage)
            m_model->updatePlaylistCoverImage(m_folderId, *imagePath);
        if (newName != m_folderName) {
            // Update m_folderName first: renamePlaylist emits tracksChanged,
            // which refreshes this page and must use the new name.
            m_folderName = newName;
            m_model->renamePlaylist(m_folderId, newName);
        }
        delete c1; delete c2; delete imagePath;
        dlg->accept();
    });
    btnRow->addWidget(saveBtn);
    layout->addLayout(btnRow);

    connect(nameEdit, &QLineEdit::returnPressed, saveBtn, &QPushButton::click);
    dlg->exec();
}

void FolderDetailPage::showMoveDialog(int trackId) {
    // "Adicionar a playlist" with checkable membership (N:N, decision 2).
    auto playlists = m_model->folders();
    const QList<int> memberOf = m_model->playlistIdsForTrack(trackId);

    auto *menu = new QMenu(this);
    lumen::design::StyleSheet::apply(menu, QString(
        "QMenu { background: %1; border: 1px solid %2; border-radius: 8px; padding: 4px; color: %3; }"
        "QMenu::item { padding: 8px 16px; border-radius: 4px; }"
        "QMenu::item:selected { background: %4; }"
        "QMenu::separator { height: 1px; background: %2; margin: 4px 0; }"
    ).arg(Theme::card().name(), Theme::border().name(), Theme::text().name(), Theme::cardHover().name()));

    auto *header = menu->addAction(Lang::tr("Adicionar à playlist"));
    header->setEnabled(false);
    menu->addSeparator();

    bool addedAny = false;
    for (auto &f : playlists) {
        const bool isMember = memberOf.contains(f.id);
        auto *act = menu->addAction(f.name);
        act->setCheckable(true);
        act->setChecked(isMember);
        const int targetId = f.id;
        connect(act, &QAction::triggered, this, [this, trackId, targetId, isMember](bool) {
            if (isMember)
                m_model->removeTrackFromPlaylist(trackId, targetId);
            else
                m_model->addTrackToPlaylist(trackId, targetId);
        });
        addedAny = true;
    }

    if (m_folderId != 0 && memberOf.contains(m_folderId)) {
        if (addedAny) menu->addSeparator();
        menu->addAction(Lang::tr("Remover da playlist"), [this, trackId]() {
            m_model->removeTrackFromPlaylist(trackId, m_folderId);
        });
    }

    if (!addedAny) {
        menu->addAction(Lang::tr("Nenhuma playlist disponível"))->setEnabled(false);
    }

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void FolderDetailPage::showCoverLightbox(const QString &imagePath) {
    QPixmap full(imagePath);
    if (full.isNull()) return;

    auto *dlg = new QDialog(this);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setAttribute(Qt::WA_TranslucentBackground);
    dlg->setModal(true);
    lumen::design::StyleSheet::apply(dlg, "QDialog { background: rgba(0,0,0,0.88); }");

    // Cover the whole application window.
    QWidget *top = window();
    if (top) dlg->setGeometry(QRect(top->mapToGlobal(QPoint(0, 0)), top->size()));

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(18);

    // Backdrop click closes the lightbox.
    auto *backdrop = new QPushButton(dlg);
    backdrop->setGeometry(0, 0, dlg->width(), dlg->height());
    lumen::design::StyleSheet::apply(backdrop, "background: transparent; border: none;");
    backdrop->setCursor(Qt::PointingHandCursor);
    backdrop->lower();
    connect(backdrop, &QPushButton::clicked, dlg, &QDialog::accept);

    layout->addStretch();

    int side = top ? qMin(top->height() - 200, top->width() - 120) : 520;
    side = qBound(240, side, 640);
    auto *imgLabel = new QLabel();
    imgLabel->setAlignment(Qt::AlignCenter);
    lumen::design::StyleSheet::apply(imgLabel, "background: transparent;");
    imgLabel->setPixmap(full.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(imgLabel, 0, Qt::AlignCenter);

    auto *closeBtn = new QPushButton(Lang::tr("Fechar"));
    closeBtn->setFont(Theme::bodyFont(13));
    closeBtn->setFixedHeight(38);
    closeBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(closeBtn, 
        "QPushButton { background: transparent; color: white; border: none; padding: 0 24px; font-weight: bold; }"
        "QPushButton:hover { color: rgba(255,255,255,0.7); }");
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignCenter);

    layout->addStretch();
    dlg->exec();
}
