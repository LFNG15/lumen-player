#include "likedpage.h"
#include "lang.h"
#include "theme.h"
#include "design/stylesheet.h"
#include "design/icons.h"
#include "models/tracklistmodel.h"
#include "models/trackrowdelegate.h"
#include "models/trackcontextmenu.h"

#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListView>
#include <QShortcut>
#include <QCursor>

namespace Icons = lumen::design::Icons;

LikedPage::LikedPage(TrackModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_header = new QWidget(this);
    lumen::design::StyleSheet::apply(m_header, QStringLiteral("background: transparent;"));
    root->addWidget(m_header);

    m_listModel = new TrackListModel(m_model, this);
    TrackListModel::Source src;
    src.kind = TrackListModel::Source::Liked;
    m_listModel->setSource(src);
    m_listModel->setReorderEnabled(false);

    m_view = new QListView(this);
    m_view->setModel(m_listModel);
    m_view->setUniformItemSizes(true);
    m_view->setLayoutMode(QListView::Batched);
    m_view->setResizeMode(QListView::Adjust);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setMouseTracking(true);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setSpacing(8);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    lumen::design::StyleSheet::apply(m_view, QStringLiteral(
        "QListView { background: transparent; border: none; outline: none; }"));

    m_delegate = new TrackRowDelegate(m_view);
    m_view->setItemDelegate(m_delegate);
    m_ctx = new TrackContextMenu(m_model, this);

    connect(m_delegate, &TrackRowDelegate::playClicked, this, [this](const QModelIndex &idx) {
        const int id = m_listModel->trackIdAt(idx.row());
        if (Track *t = m_model->findTrack(id)) emit playRequested(*t);
    });
    connect(m_delegate, &TrackRowDelegate::likeClicked, this, [this](const QModelIndex &idx) {
        const int id = m_listModel->trackIdAt(idx.row());
        if (id > 0) emit likeToggled(id);
    });
    connect(m_delegate, &TrackRowDelegate::moreClicked, this,
            [this](const QModelIndex &idx, const QPoint &gp) {
        QList<int> ids = selectedIds();
        if (ids.isEmpty()) {
            const int id = m_listModel->trackIdAt(idx.row());
            if (id > 0) ids.append(id);
        }
        if (!ids.isEmpty())
            m_ctx->popupAddMenu(ids, gp);
    });
    connect(m_view, &QListView::customContextMenuRequested, this, [this](const QPoint &pos) {
        showContext(m_view->viewport()->mapToGlobal(pos));
    });

    connect(m_ctx, &TrackContextMenu::playRequested, this, &LikedPage::playRequested);
    connect(m_ctx, &TrackContextMenu::enqueueRequested, this, &LikedPage::enqueueRequested);
    connect(m_ctx, &TrackContextMenu::editRequested, this, &LikedPage::editTrackRequested);
    connect(m_ctx, &TrackContextMenu::deleteRequested, this, &LikedPage::deleteRequested);
    connect(m_ctx, &TrackContextMenu::likeToggled, this, &LikedPage::likeToggled);

    auto *space = new QShortcut(Qt::Key_Space, m_view);
    connect(space, &QShortcut::activated, this, [this]() {
        auto ids = selectedIds();
        if (!ids.isEmpty())
            if (Track *t = m_model->findTrack(ids.first()))
                emit playRequested(*t);
    });

    root->addWidget(m_view, 1);
}

QList<int> LikedPage::selectedIds() const
{
    QList<int> ids;
    for (const QModelIndex &idx : m_view->selectionModel()->selectedRows()) {
        const int id = m_listModel->trackIdAt(idx.row());
        if (id > 0) ids.append(id);
    }
    if (ids.isEmpty() && m_view->currentIndex().isValid()) {
        const int id = m_listModel->trackIdAt(m_view->currentIndex().row());
        if (id > 0) ids.append(id);
    }
    return ids;
}

void LikedPage::showContext(const QPoint &globalPos)
{
    auto ids = selectedIds();
    if (!ids.isEmpty())
        m_ctx->popup(ids, globalPos);
}

static void clearLayoutTreeLiked(QLayout *layout)
{
    if (!layout) return;
    while (QLayoutItem *it = layout->takeAt(0)) {
        if (QWidget *w = it->widget()) {
            w->hide();
            delete w;
        } else if (QLayout *sub = it->layout()) {
            clearLayoutTreeLiked(sub);
        }
        delete it;
    }
}

void LikedPage::rebuildHeader()
{
    // Same nested-layout orphan bug as FolderDetailPage (playlist switch overlay).
    if (QLayout *old = m_header->layout()) {
        clearLayoutTreeLiked(old);
        delete old;
    }
    const auto kids = m_header->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *w : kids)
        delete w;

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
    connect(backBtn, &QPushButton::clicked, this, &LikedPage::navigateBack);
    lay->addWidget(backBtn, 0, Qt::AlignLeft);

    auto *row = new QHBoxLayout();
    row->setSpacing(20);

    auto *cover = new QWidget();
    cover->setFixedSize(140, 140);
    lumen::design::StyleSheet::apply(cover, QString(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 10px;"
    ).arg(Theme::accent().name(), Theme::danger().name()));
    auto *heart = new QLabel(Icons::heart(), cover);
    heart->setFont(Theme::iconFont(36));
    lumen::design::StyleSheet::apply(heart, QString(
        "color: %1; background: transparent;").arg(Theme::text().name()));
    heart->setAlignment(Qt::AlignCenter);
    heart->setGeometry(0, 0, 140, 140);
    row->addWidget(cover);

    auto *info = new QVBoxLayout();
    info->addStretch();
    auto *type = new QLabel(Lang::tr("COLEÇÃO"));
    type->setFont(Theme::bodyFont(10));
    lumen::design::StyleSheet::apply(type, QString(
        "color: %1; background: transparent; font-weight: bold; letter-spacing: 1px;"
    ).arg(Theme::textMuted().name()));
    info->addWidget(type);

    auto *name = new QLabel(Lang::tr("Curtidas"));
    name->setFont(Theme::titleFont(32));
    lumen::design::StyleSheet::apply(name, QString(
        "color: %1; background: transparent;").arg(Theme::text().name()));
    info->addWidget(name);

    const int n = m_listModel->rowCount();
    auto *stats = new QLabel(QString(Lang::tr("%1 faixa%2"))
        .arg(n).arg(n != 1 ? "s" : ""));
    stats->setFont(Theme::bodyFont(12));
    lumen::design::StyleSheet::apply(stats, QString(
        "color: %1; background: transparent;").arg(Theme::textSoft().name()));
    info->addWidget(stats);
    info->addStretch();
    row->addLayout(info, 1);
    lay->addLayout(row);

    auto *playBtn = new QPushButton(QStringLiteral("  %1  %2")
        .arg(Icons::play(), Lang::tr("Tocar curtidas")));
    playBtn->setObjectName(QStringLiteral("lumenAccentBtn"));
    playBtn->setCursor(Qt::PointingHandCursor);
    playBtn->setFixedHeight(40);
    connect(playBtn, &QPushButton::clicked, this, [this]() {
        const auto tracks = m_listModel->tracksInOrder();
        if (!tracks.isEmpty())
            emit playRequested(tracks.first());
    });
    lay->addWidget(playBtn, 0, Qt::AlignLeft);
}

void LikedPage::refresh(int currentTrackId, bool isPlaying)
{
    m_listModel->reload();
    m_listModel->setPlaybackState(currentTrackId, isPlaying);
    rebuildHeader();
}
