#include "folderdetailpage.h"
#include "database.h"
#include "lang.h"
#include "theme.h"
#include "textutils.h"
#include "coverwidget.h"
#include "design/stylesheet.h"
#include "design/icons.h"
#include "models/tracklistmodel.h"
#include "models/trackfilterproxy.h"
#include "models/trackrowdelegate.h"
#include "models/trackcontextmenu.h"

#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListView>
#include <QLineEdit>
#include <QMenu>
#include <QDialog>
#include <QColorDialog>
#include <QFileDialog>
#include <QGridLayout>
#include <QSettings>
#include <QKeyEvent>
#include <QShortcut>
#include <QCursor>
#include <QFrame>
#include <QPalette>
#include <algorithm>

namespace Icons = lumen::design::Icons;

struct SortMode { const char *id; const char *label; };
static const SortMode kSortModes[] = {
    {"custom",   "Personalizada"},
    {"title",    "Título"},
    {"artist",   "Artista"},
    {"recent",   "Adicionadas recentemente"},
    {"oldest",   "Mais antigas"},
    {"duration", "Duração"},
};

static void applySortMode(QList<Track> &tracks, const QString &mode)
{
    auto title  = [](const Track &t) { return TextUtils::normalized(t.title); };
    auto artist = [](const Track &t) { return TextUtils::normalized(t.artist); };

    if (mode == QLatin1String("title")) {
        std::sort(tracks.begin(), tracks.end(),
            [&](const Track &a, const Track &b) { return title(a) < title(b); });
    } else if (mode == QLatin1String("artist")) {
        std::sort(tracks.begin(), tracks.end(),
            [&](const Track &a, const Track &b) {
                return artist(a) != artist(b) ? artist(a) < artist(b)
                                              : title(a) < title(b);
            });
    } else if (mode == QLatin1String("recent")) {
        std::sort(tracks.begin(), tracks.end(),
            [](const Track &a, const Track &b) {
                return a.addedAt != b.addedAt ? a.addedAt > b.addedAt : a.id > b.id;
            });
    } else if (mode == QLatin1String("oldest")) {
        std::sort(tracks.begin(), tracks.end(),
            [](const Track &a, const Track &b) {
                return a.addedAt != b.addedAt ? a.addedAt < b.addedAt : a.id < b.id;
            });
    } else if (mode == QLatin1String("duration")) {
        std::sort(tracks.begin(), tracks.end(),
            [&](const Track &a, const Track &b) {
                return a.durationMs != b.durationMs ? a.durationMs < b.durationMs
                                                    : title(a) < title(b);
            });
    }
}

FolderDetailPage::FolderDetailPage(TrackModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_header = new QWidget(this);
    lumen::design::StyleSheet::apply(m_header, QStringLiteral("background: transparent;"));
    root->addWidget(m_header);
    setupHeaderUi();

    m_listModel = new TrackListModel(m_model, this);
    m_proxy = new TrackFilterProxy(this);
    m_proxy->setSourceModel(m_listModel);

    m_view = new QListView(this);
    m_view->setModel(m_proxy);
    m_view->setUniformItemSizes(true);
    m_view->setLayoutMode(QListView::Batched);
    m_view->setResizeMode(QListView::Adjust);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setMouseTracking(true);
    m_view->setDropIndicatorShown(true);
    m_view->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setSpacing(8);
    lumen::design::StyleSheet::apply(m_view, QStringLiteral(
        "QListView { background: transparent; border: none; outline: none; }"));

    m_delegate = new TrackRowDelegate(m_view);
    m_view->setItemDelegate(m_delegate);

    m_ctx = new TrackContextMenu(m_model, this);

    connect(m_delegate, &TrackRowDelegate::playClicked, this, [this](const QModelIndex &proxyIdx) {
        const QModelIndex src = m_proxy->mapToSource(proxyIdx);
        const int id = m_listModel->trackIdAt(src.row());
        if (Track *t = m_model->findTrack(id))
            emit playRequested(*t);
    });
    connect(m_delegate, &TrackRowDelegate::likeClicked, this, [this](const QModelIndex &proxyIdx) {
        const QModelIndex src = m_proxy->mapToSource(proxyIdx);
        const int id = m_listModel->trackIdAt(src.row());
        if (id > 0) emit likeToggled(id);
    });
    // "⋯" / add affordance: queue + playlist (full menu stays on right-click).
    connect(m_delegate, &TrackRowDelegate::moreClicked, this,
            [this](const QModelIndex &proxyIdx, const QPoint &gp) {
        QList<int> ids = selectedTrackIds();
        if (ids.isEmpty()) {
            const QModelIndex src = m_proxy->mapToSource(proxyIdx);
            const int id = m_listModel->trackIdAt(src.row());
            if (id > 0) ids.append(id);
        }
        if (!ids.isEmpty())
            m_ctx->popupAddMenu(ids, gp);
    });

    connect(m_view, &QListView::customContextMenuRequested, this, [this](const QPoint &pos) {
        showContextForSelection(m_view->viewport()->mapToGlobal(pos));
    });
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);

    // Keyboard: Space play, L like, Q enqueue, Del remove, Menu context.
    auto *space = new QShortcut(Qt::Key_Space, m_view);
    connect(space, &QShortcut::activated, this, [this]() {
        const auto ids = selectedTrackIds();
        if (ids.isEmpty()) return;
        if (Track *t = m_model->findTrack(ids.first()))
            emit playRequested(*t);
    });
    auto *likeKey = new QShortcut(Qt::Key_L, m_view);
    connect(likeKey, &QShortcut::activated, this, [this]() {
        for (int id : selectedTrackIds())
            emit likeToggled(id);
    });
    auto *qKey = new QShortcut(Qt::Key_Q, m_view);
    connect(qKey, &QShortcut::activated, this, [this]() {
        for (int id : selectedTrackIds())
            if (Track *t = m_model->findTrack(id))
                emit enqueueRequested(*t);
    });
    auto *delKey = new QShortcut(QKeySequence::Delete, m_view);
    connect(delKey, &QShortcut::activated, this, [this]() {
        for (int id : selectedTrackIds())
            emit deleteRequested(id);
    });
    auto *menuKey = new QShortcut(Qt::Key_Menu, m_view);
    connect(menuKey, &QShortcut::activated, this, [this]() {
        showContextForSelection(QCursor::pos());
    });

    connect(m_ctx, &TrackContextMenu::playRequested, this, &FolderDetailPage::playRequested);
    connect(m_ctx, &TrackContextMenu::enqueueRequested, this, &FolderDetailPage::enqueueRequested);
    connect(m_ctx, &TrackContextMenu::editRequested, this, &FolderDetailPage::editTrackRequested);
    connect(m_ctx, &TrackContextMenu::deleteRequested, this, &FolderDetailPage::deleteRequested);
    connect(m_ctx, &TrackContextMenu::likeToggled, this, &FolderDetailPage::likeToggled);

    root->addWidget(m_view, 1);
}

QList<Track> FolderDetailPage::displayedTracks() const
{
    return m_listModel ? m_listModel->tracksInOrder() : QList<Track>{};
}

QList<int> FolderDetailPage::selectedTrackIds() const
{
    QList<int> ids;
    if (!m_view || !m_proxy || !m_listModel) return ids;
    for (const QModelIndex &px : m_view->selectionModel()->selectedRows()) {
        const QModelIndex src = m_proxy->mapToSource(px);
        const int id = m_listModel->trackIdAt(src.row());
        if (id > 0) ids.append(id);
    }
    // Fall back to current index if nothing selected.
    if (ids.isEmpty() && m_view->currentIndex().isValid()) {
        const QModelIndex src = m_proxy->mapToSource(m_view->currentIndex());
        const int id = m_listModel->trackIdAt(src.row());
        if (id > 0) ids.append(id);
    }
    return ids;
}

void FolderDetailPage::showContextForSelection(const QPoint &globalPos)
{
    const auto ids = selectedTrackIds();
    if (ids.isEmpty()) return;
    m_ctx->popup(ids, globalPos);
}

QString FolderDetailPage::sortMode() const
{
    if (m_folderId > 0)
        return Database::instance().playlistSortMode(m_folderId);
    return QSettings().value(QStringLiteral("playlistSort/%1").arg(m_folderId),
                             QStringLiteral("custom")).toString();
}

void FolderDetailPage::setFolder(const QString &folderName)
{
    m_folderName = folderName;
    m_filterText.clear();
    m_folderId = 0;
    if (!folderName.isEmpty()) {
        for (const auto &f : m_model->folders()) {
            if (f.name == folderName) {
                m_folderId = f.id;
                break;
            }
        }
    }
}

void FolderDetailPage::updateReorderFlag()
{
    const bool isStandalone = m_folderName.isEmpty();
    m_canReorder = !isStandalone
        && sortMode() == QLatin1String("custom")
        && m_filterText.isEmpty();
    m_listModel->setReorderEnabled(m_canReorder);
    m_view->setDragDropMode(m_canReorder
        ? QAbstractItemView::InternalMove
        : QAbstractItemView::NoDragDrop);
    m_view->setDefaultDropAction(Qt::MoveAction);
}

void FolderDetailPage::applySortToModel()
{
    const bool isStandalone = m_folderName.isEmpty();
    TrackListModel::Source src;
    if (isStandalone) {
        src.kind = TrackListModel::Source::Standalone;
    } else {
        src.kind = TrackListModel::Source::Playlist;
        src.playlistId = m_folderId;
        src.playlistName = m_folderName;
    }
    m_listModel->setSource(src);

    // Client-side sort modes other than custom re-order ids without writing.
    const QString mode = sortMode();
    if (mode != QLatin1String("custom")) {
        QList<Track> tracks = m_listModel->tracksInOrder();
        applySortMode(tracks, mode);
        QVector<int> ids;
        ids.reserve(tracks.size());
        for (const auto &t : tracks)
            ids.append(t.id);
        m_listModel->setOrderedIds(ids);
    }
    updateReorderFlag();
}

void FolderDetailPage::setupHeaderUi()
{
    // Built once — updateHeader() only mutates text/cover. Rebuilding the whole
    // tree left orphan QLabels (playlist names) stacked on top of each other.
    auto *lay = new QVBoxLayout(m_header);
    lay->setContentsMargins(32, 28, 32, 12);
    lay->setSpacing(12);

    auto *backBtn = new QPushButton(Icons::back(), m_header);
    backBtn->setFixedSize(34, 34);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFont(Theme::iconFont(12));
    lumen::design::StyleSheet::apply(backBtn, QString(
        "QPushButton { background: rgba(255,255,255,0.05); color: %1; border: none; border-radius: 17px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }"
    ).arg(Theme::text().name()));
    connect(backBtn, &QPushButton::clicked, this, &FolderDetailPage::navigateBack);
    lay->addWidget(backBtn, 0, Qt::AlignLeft);

    auto *headerRow = new QHBoxLayout();
    headerRow->setSpacing(20);

    m_coverHost = new QWidget(m_header);
    m_coverHost->setFixedSize(140, 140);
    auto *coverLay = new QVBoxLayout(m_coverHost);
    coverLay->setContentsMargins(0, 0, 0, 0);
    coverLay->setSpacing(0);
    headerRow->addWidget(m_coverHost, 0, Qt::AlignTop);

    auto *info = new QVBoxLayout();
    info->setSpacing(4);
    info->addStretch();

    // IMPORTANT: do not style these QLabels with "background: transparent".
    // Qt then skips erasing the previous glyphs on setText(), so playlist
    // names stack (TESTE B under Teste C). Palette + solid fill is safe.
    m_typeLabel = new QLabel(m_header);
    m_typeLabel->setFont(Theme::bodyFont(10));
    m_typeLabel->setAutoFillBackground(true);
    info->addWidget(m_typeLabel);

    m_nameLabel = new QLabel(m_header);
    m_nameLabel->setFont(Theme::titleFont(28));
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_nameLabel->setAutoFillBackground(true);
    info->addWidget(m_nameLabel);

    m_statsLabel = new QLabel(m_header);
    m_statsLabel->setFont(Theme::bodyFont(12));
    m_statsLabel->setAutoFillBackground(true);
    info->addWidget(m_statsLabel);
    info->addStretch();
    headerRow->addLayout(info, 1);

    m_editBtn = new QPushButton(Icons::edit(), m_header);
    m_editBtn->setFixedSize(36, 36);
    m_editBtn->setCursor(Qt::PointingHandCursor);
    m_editBtn->setFont(Theme::iconFont(12));
    m_editBtn->setToolTip(Lang::tr("Editar playlist"));
    lumen::design::StyleSheet::apply(m_editBtn, QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 18px; }"
        "QPushButton:hover { color: %3; border-color: %3; }"
    ).arg(Theme::textMuted().name(), Theme::border().name(), Theme::accent().name()));
    connect(m_editBtn, &QPushButton::clicked, this, &FolderDetailPage::showEditDialog);
    headerRow->addWidget(m_editBtn, 0, Qt::AlignBottom);

    lay->addLayout(headerRow);

    auto *controls = new QHBoxLayout();
    controls->setSpacing(10);

    m_playBtn = new QPushButton(m_header);
    m_playBtn->setObjectName(QStringLiteral("lumenAccentBtn"));
    m_playBtn->setCursor(Qt::PointingHandCursor);
    m_playBtn->setFont(Theme::bodyFont(13));
    m_playBtn->setFixedHeight(40);
    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        const auto tracks = displayedTracks();
        if (!tracks.isEmpty())
            emit playRequested(tracks.first());
    });
    controls->addWidget(m_playBtn);

    m_searchEdit = new QLineEdit(m_header);
    m_searchEdit->setPlaceholderText(Lang::tr("Buscar na playlist"));
    m_searchEdit->setFont(Theme::bodyFont(12));
    m_searchEdit->setFixedHeight(36);
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &t) {
        m_filterText = t;
        if (m_proxy)
            m_proxy->setNeedle(TextUtils::normalized(t));
        updateReorderFlag();
    });
    controls->addWidget(m_searchEdit, 1);

    auto *sortBtn = new QPushButton(Icons::sort(), m_header);
    sortBtn->setFixedSize(36, 36);
    sortBtn->setCursor(Qt::PointingHandCursor);
    sortBtn->setFont(Theme::iconFont(12));
    sortBtn->setToolTip(Lang::tr("Ordenar"));
    lumen::design::StyleSheet::apply(sortBtn, QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 18px; }"
        "QPushButton:hover { color: %3; }"
    ).arg(Theme::textMuted().name(), Theme::border().name(), Theme::text().name()));
    connect(sortBtn, &QPushButton::clicked, this, &FolderDetailPage::showSortMenu);
    controls->addWidget(sortBtn);

    lay->addLayout(controls);
}

void FolderDetailPage::replaceCover(QWidget *cover)
{
    if (!m_coverHost || !cover) {
        delete cover;
        return;
    }
    if (QLayout *lay = m_coverHost->layout()) {
        while (QLayoutItem *it = lay->takeAt(0)) {
            if (QWidget *w = it->widget())
                delete w;
            delete it;
        }
        lay->addWidget(cover);
    } else {
        cover->setParent(m_coverHost);
        cover->setGeometry(0, 0, 140, 140);
        cover->show();
    }
}

static void paintLabel(QLabel *lab, const QColor &fg, const QColor &bg)
{
    if (!lab) return;
    // Palette only — QSS styling belongs in src/design/ (CI gate).
    QPalette pal = lab->palette();
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::WindowText, fg);
    pal.setColor(QPalette::Text, fg);
    lab->setPalette(pal);
    lab->setAutoFillBackground(true);
}

void FolderDetailPage::updateHeader()
{
    const bool isStandalone = m_folderName.isEmpty();
    const auto tracks = displayedTracks();
    const QColor pageBg = Theme::bg();

    paintLabel(m_typeLabel, Theme::textMuted(), pageBg);
    paintLabel(m_nameLabel, Theme::text(), pageBg);
    paintLabel(m_statsLabel, Theme::textSoft(), pageBg);

    m_typeLabel->setText(isStandalone ? Lang::tr("MÚSICAS AVULSAS") : Lang::tr("PLAYLIST"));
    // Clear first so any residual buffer is wiped before the new name.
    m_nameLabel->clear();
    m_nameLabel->setText(isStandalone ? Lang::tr("Músicas avulsas") : m_folderName);

    qint64 totalMs = 0;
    for (const auto &t : tracks)
        totalMs += t.durationMs;
    m_statsLabel->setText(QString(Lang::tr("%1 faixa%2%3"))
        .arg(tracks.size())
        .arg(tracks.size() != 1 ? QStringLiteral("s") : QString())
        .arg(totalMs > 0 ? QStringLiteral(" · %1").arg(Theme::formatTime(totalMs)) : QString()));

    m_editBtn->setVisible(!isStandalone);
    m_playBtn->setText(QStringLiteral("  %1  %2").arg(Icons::play(), Lang::tr("Tocar")));

    // Avoid re-emitting textChanged when restoring the same filter.
    if (m_searchEdit->text() != m_filterText)
        m_searchEdit->setText(m_filterText);

    QWidget *cover = nullptr;
    if (isStandalone) {
        cover = new QWidget(m_coverHost);
        cover->setFixedSize(140, 140);
        lumen::design::StyleSheet::apply(cover, QString(
            "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 10px;"
        ).arg(Theme::accent().name(), Theme::danger().name()));
    } else {
        Folder folder;
        for (const auto &f : m_model->folders()) {
            if (f.id == m_folderId) { folder = f; break; }
        }
        cover = CoverWidget::playlistCover(folder, tracks, 140, 10, m_coverHost);
    }
    replaceCover(cover);
}

void FolderDetailPage::refresh(int currentTrackId, bool isPlaying)
{
    m_lastCurrentId = currentTrackId;
    m_lastPlaying = isPlaying;

    applySortToModel();
    m_listModel->setPlaybackState(currentTrackId, isPlaying);
    m_proxy->setNeedle(TextUtils::normalized(m_filterText));
    updateHeader();
}

void FolderDetailPage::showSortMenu()
{
    auto *menu = new QMenu(this);
    lumen::design::StyleSheet::apply(menu, QString(
        "QMenu { background: %1; border: 1px solid %2; border-radius: 8px; padding: 4px; color: %3; }"
        "QMenu::item { padding: 8px 16px; border-radius: 4px; }"
        "QMenu::item:selected { background: %4; }"
    ).arg(Theme::card().name(), Theme::border().name(), Theme::text().name(), Theme::cardHover().name()));

    const QString current = sortMode();
    for (const auto &m : kSortModes) {
        QString label = Lang::tr(m.label);
        if (current == m.id) label = QStringLiteral("✓ ") + label;
        const QString id = m.id;
        menu->addAction(label, this, [this, id]() {
            if (m_folderId > 0)
                Database::instance().setPlaylistSortMode(m_folderId, id);
            else
                QSettings().setValue(QStringLiteral("playlistSort/%1").arg(m_folderId), id);
            refresh(m_lastCurrentId, m_lastPlaying);
        });
    }
    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void FolderDetailPage::showEditDialog()
{
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

    layout->addWidget(new QLabel(Lang::tr("Nome da playlist")));
    auto *nameEdit = new QLineEdit(m_folderName);
    nameEdit->setFont(Theme::bodyFont(13));
    nameEdit->selectAll();
    layout->addWidget(nameEdit);

    Theme::GradientPair currentCover;
    QString currentImage;
    for (const auto &f : m_model->folders()) {
        if (f.name == m_folderName) {
            currentCover = f.cover;
            currentImage = f.coverImage;
            break;
        }
    }

    layout->addWidget(new QLabel(Lang::tr("Cores da capa:")));
    auto *c1 = new QColor(currentCover.c1.isValid() ? currentCover.c1 : Theme::accent());
    auto *c2 = new QColor(currentCover.c2.isValid() ? currentCover.c2 : Theme::danger());

    auto *colorRow = new QHBoxLayout();
    auto *preview = new QWidget();
    preview->setFixedSize(50, 32);
    lumen::design::StyleSheet::apply(preview, QString(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 6px;"
    ).arg(c1->name(), c2->name()));
    colorRow->addWidget(preview);

    auto updatePrev = [preview, c1, c2]() {
        lumen::design::StyleSheet::apply(preview, QString(
            "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 6px;"
        ).arg(c1->name(), c2->name()));
    };

    auto *btn1 = new QPushButton(Lang::tr("Cor 1"));
    btn1->setFixedSize(60, 32);
    btn1->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(btn1, QString(
        "background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px; color: white;"
    ).arg(c1->name()));
    connect(btn1, &QPushButton::clicked, dlg, [btn1, c1, updatePrev, dlg]() {
        QColor chosen = QColorDialog::getColor(*c1, dlg, Lang::tr("Cor 1"));
        if (chosen.isValid()) {
            *c1 = chosen;
            lumen::design::StyleSheet::apply(btn1, QString(
                "background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px; color: white;"
            ).arg(c1->name()));
            updatePrev();
        }
    });
    colorRow->addWidget(btn1);

    auto *btn2 = new QPushButton(Lang::tr("Cor 2"));
    btn2->setFixedSize(60, 32);
    btn2->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(btn2, QString(
        "background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px; color: white;"
    ).arg(c2->name()));
    connect(btn2, &QPushButton::clicked, dlg, [btn2, c2, updatePrev, dlg]() {
        QColor chosen = QColorDialog::getColor(*c2, dlg, Lang::tr("Cor 2"));
        if (chosen.isValid()) {
            *c2 = chosen;
            lumen::design::StyleSheet::apply(btn2, QString(
                "background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px; color: white;"
            ).arg(c2->name()));
            updatePrev();
        }
    });
    colorRow->addWidget(btn2);
    colorRow->addStretch();
    layout->addLayout(colorRow);

    layout->addWidget(new QLabel(Lang::tr("Imagem da capa:")));
    auto *imagePath = new QString(currentImage);
    auto *imageRow = new QHBoxLayout();
    auto *imgPreview = new QLabel();
    imgPreview->setFixedSize(50, 32);
    if (!currentImage.isEmpty()) {
        QPixmap pm = Theme::roundedCover(currentImage, 50, 32, 6);
        if (!pm.isNull()) imgPreview->setPixmap(pm);
    }
    imageRow->addWidget(imgPreview);

    auto *pickImageBtn = new QPushButton(Lang::tr("Escolher imagem"));
    pickImageBtn->setCursor(Qt::PointingHandCursor);
    connect(pickImageBtn, &QPushButton::clicked, dlg, [dlg, imagePath, imgPreview]() {
        QString file = QFileDialog::getOpenFileName(
            dlg, Lang::tr("Escolher imagem da capa"), QString(),
            Lang::tr("Imagens (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (file.isEmpty()) return;
        *imagePath = file;
        QPixmap pm = Theme::roundedCover(file, 50, 32, 6);
        if (!pm.isNull()) imgPreview->setPixmap(pm);
    });
    imageRow->addWidget(pickImageBtn);

    auto *clearImageBtn = new QPushButton(Lang::tr("Remover"));
    clearImageBtn->setCursor(Qt::PointingHandCursor);
    connect(clearImageBtn, &QPushButton::clicked, dlg, [imagePath, imgPreview]() {
        imagePath->clear();
        imgPreview->setPixmap(QPixmap());
    });
    imageRow->addWidget(clearImageBtn);
    imageRow->addStretch();
    layout->addLayout(imageRow);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(Lang::tr("Cancelar"));
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, dlg, [dlg, c1, c2, imagePath]() {
        delete c1; delete c2; delete imagePath; dlg->reject();
    });
    btnRow->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton(Lang::tr("Salvar"));
    saveBtn->setObjectName(QStringLiteral("lumenAccentBtn"));
    saveBtn->setCursor(Qt::PointingHandCursor);
    connect(saveBtn, &QPushButton::clicked, this,
            [this, dlg, nameEdit, c1, c2, imagePath, currentImage]() {
        QString newName = nameEdit->text().trimmed();
        if (newName.isEmpty()) return;
        m_model->updatePlaylistCover(m_folderId, *c1, *c2);
        if (*imagePath != currentImage)
            m_model->updatePlaylistCoverImage(m_folderId, *imagePath);
        if (newName != m_folderName) {
            m_folderName = newName;
            m_model->renamePlaylist(m_folderId, newName);
        }
        delete c1; delete c2; delete imagePath;
        dlg->accept();
        refresh(m_lastCurrentId, m_lastPlaying);
    });
    btnRow->addWidget(saveBtn);
    layout->addLayout(btnRow);
    dlg->exec();
}

void FolderDetailPage::showCoverLightbox(const QString &imagePath)
{
    QPixmap full(imagePath);
    if (full.isNull()) return;

    auto *dlg = new QDialog(this);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setAttribute(Qt::WA_TranslucentBackground);
    dlg->setModal(true);
    lumen::design::StyleSheet::apply(dlg, QStringLiteral("QDialog { background: rgba(0,0,0,0.88); }"));

    if (QWidget *top = window())
        dlg->setGeometry(QRect(top->mapToGlobal(QPoint(0, 0)), top->size()));

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->addStretch();

    int side = window() ? qMin(window()->height() - 200, window()->width() - 120) : 520;
    side = qBound(240, side, 640);
    auto *imgLabel = new QLabel();
    imgLabel->setAlignment(Qt::AlignCenter);
    imgLabel->setPixmap(full.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(imgLabel, 0, Qt::AlignCenter);

    auto *closeBtn = new QPushButton(Lang::tr("Fechar"));
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignCenter);
    layout->addStretch();
    dlg->exec();
}
