#include "librarypage.h"
#include "lang.h"
#include "theme.h"
#include "design/stylesheet.h"
#include "design/icons.h"
#include "models/tracklistmodel.h"
#include "models/trackrowdelegate.h"
#include "models/trackcontextmenu.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QListView>
#include <QShortcut>
#include <QFrame>
#include <QCursor>

LibraryPage::LibraryPage(TrackModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_header = new QWidget(this);
    lumen::design::StyleSheet::apply(m_header, QStringLiteral("background: transparent;"));
    auto *headerLayout = new QVBoxLayout(m_header);
    headerLayout->setContentsMargins(32, 28, 32, 12);
    headerLayout->setSpacing(4);

    auto *backBtn = new QPushButton(lumen::design::Icons::back(), m_header);
    backBtn->setFixedSize(34, 34);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFont(Theme::iconFont(12));
    lumen::design::StyleSheet::apply(backBtn, QString(
        "QPushButton { background: " + Theme::hoverBg(0.05) + "; color: %1; border: none; border-radius: 17px; }"
        "QPushButton:hover { background: " + Theme::hoverBg(0.1) + "; }"
    ).arg(Theme::text().name()));
    connect(backBtn, &QPushButton::clicked, this, &LibraryPage::navigateBack);
    headerLayout->addWidget(backBtn, 0, Qt::AlignLeft);

    auto *title = new QLabel(m_header);
    Lang::bindText(title, QStringLiteral("Biblioteca Completa"));
    title->setFont(Theme::titleFont(28));
    lumen::design::StyleSheet::apply(title, QString(
        "color: %1; background: transparent;").arg(Theme::text().name()));
    headerLayout->addWidget(title);

    m_countLabel = new QLabel(m_header);
    Lang::bind(m_countLabel, [this]() {
        const int n = m_listModel ? m_listModel->rowCount() : 0;
        m_countLabel->setText(QString(Lang::tr("%1 faixa%2")).arg(n).arg(n != 1 ? "s" : ""));
    });
    m_countLabel->setFont(Theme::bodyFont(12));
    lumen::design::StyleSheet::apply(m_countLabel, QString(
        "color: %1; background: transparent;").arg(Theme::textSoft().name()));
    headerLayout->addWidget(m_countLabel);

    root->addWidget(m_header);

    m_listModel = new TrackListModel(m_model, this);
    TrackListModel::Source src;
    src.kind = TrackListModel::Source::All;
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
    m_view->setSpacing(2);
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

    connect(m_ctx, &TrackContextMenu::playRequested, this, &LibraryPage::playRequested);
    connect(m_ctx, &TrackContextMenu::enqueueRequested, this, &LibraryPage::enqueueRequested);
    connect(m_ctx, &TrackContextMenu::editRequested, this, &LibraryPage::editTrackRequested);
    connect(m_ctx, &TrackContextMenu::deleteRequested, this, &LibraryPage::deleteRequested);
    connect(m_ctx, &TrackContextMenu::likeToggled, this, &LibraryPage::likeToggled);

    auto *space = new QShortcut(Qt::Key_Space, m_view);
    connect(space, &QShortcut::activated, this, [this]() {
        auto ids = selectedIds();
        if (!ids.isEmpty())
            if (Track *t = m_model->findTrack(ids.first()))
                emit playRequested(*t);
    });
    auto *likeKey = new QShortcut(Qt::Key_L, m_view);
    connect(likeKey, &QShortcut::activated, this, [this]() {
        for (int id : selectedIds()) emit likeToggled(id);
    });
    auto *qKey = new QShortcut(Qt::Key_Q, m_view);
    connect(qKey, &QShortcut::activated, this, [this]() {
        for (int id : selectedIds())
            if (Track *t = m_model->findTrack(id)) emit enqueueRequested(*t);
    });
    auto *delKey = new QShortcut(QKeySequence::Delete, m_view);
    connect(delKey, &QShortcut::activated, this, [this]() {
        for (int id : selectedIds()) emit deleteRequested(id);
    });
    auto *menuKey = new QShortcut(Qt::Key_Menu, m_view);
    connect(menuKey, &QShortcut::activated, this, [this]() {
        showContext(QCursor::pos());
    });

    root->addWidget(m_view, 1);
}

QList<int> LibraryPage::selectedIds() const
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

void LibraryPage::showContext(const QPoint &globalPos)
{
    auto ids = selectedIds();
    if (!ids.isEmpty())
        m_ctx->popup(ids, globalPos);
}

void LibraryPage::refresh(int currentTrackId, bool isPlaying)
{
    m_listModel->reload();
    m_listModel->setPlaybackState(currentTrackId, isPlaying);
    const int n = m_listModel->rowCount();
    m_countLabel->setText(QString(Lang::tr("%1 faixa%2")).arg(n).arg(n != 1 ? "s" : ""));
}
