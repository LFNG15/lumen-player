#include "searchpage.h"
#include "lang.h"
#include "theme.h"
#include "textutils.h"
#include "design/stylesheet.h"
#include "models/tracklistmodel.h"
#include "models/trackfilterproxy.h"
#include "models/trackrowdelegate.h"
#include "models/trackcontextmenu.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListView>
#include <QTimer>
#include <QCursor>

SearchPage::SearchPage(TrackModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(32, 28, 32, 28);
    root->setSpacing(12);

    m_title = new QLabel(this);
    m_title->setFont(Theme::titleFont(24));
    lumen::design::StyleSheet::apply(m_title, QString(
        "color: %1; background: transparent;").arg(Theme::text().name()));
    root->addWidget(m_title);

    m_empty = new QLabel(this);
    m_empty->setFont(Theme::bodyFont(14));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    lumen::design::StyleSheet::apply(m_empty, QString(
        "color: %1; background: transparent; padding-top: 40px;"
    ).arg(Theme::textMuted().name()));
    root->addWidget(m_empty);

    m_playlistHits = new QWidget(this);
    lumen::design::StyleSheet::apply(m_playlistHits, QStringLiteral("background: transparent;"));
    new QVBoxLayout(m_playlistHits); // filled in applyQuery
    root->addWidget(m_playlistHits);

    m_listModel = new TrackListModel(m_model, this);
    TrackListModel::Source src;
    src.kind = TrackListModel::Source::All;
    m_listModel->setSource(src);

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
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setSpacing(2);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    lumen::design::StyleSheet::apply(m_view, QStringLiteral(
        "QListView { background: transparent; border: none; outline: none; }"));

    m_delegate = new TrackRowDelegate(m_view);
    m_view->setItemDelegate(m_delegate);
    m_ctx = new TrackContextMenu(m_model, this);

    connect(m_delegate, &TrackRowDelegate::playClicked, this, [this](const QModelIndex &px) {
        const QModelIndex src = m_proxy->mapToSource(px);
        const int id = m_listModel->trackIdAt(src.row());
        if (Track *t = m_model->findTrack(id)) emit playRequested(*t);
    });
    connect(m_delegate, &TrackRowDelegate::likeClicked, this, [this](const QModelIndex &px) {
        const QModelIndex src = m_proxy->mapToSource(px);
        const int id = m_listModel->trackIdAt(src.row());
        if (id > 0) emit likeToggled(id);
    });
    connect(m_delegate, &TrackRowDelegate::moreClicked, this,
            [this](const QModelIndex &px, const QPoint &gp) {
        QList<int> ids;
        for (const QModelIndex &sel : m_view->selectionModel()->selectedRows()) {
            const QModelIndex src = m_proxy->mapToSource(sel);
            const int id = m_listModel->trackIdAt(src.row());
            if (id > 0) ids.append(id);
        }
        if (ids.isEmpty()) {
            const QModelIndex src = m_proxy->mapToSource(px);
            const int id = m_listModel->trackIdAt(src.row());
            if (id > 0) ids.append(id);
        }
        if (!ids.isEmpty())
            m_ctx->popupAddMenu(ids, gp);
    });
    connect(m_view, &QListView::customContextMenuRequested, this, [this](const QPoint &pos) {
        showContext(m_view->viewport()->mapToGlobal(pos));
    });

    connect(m_ctx, &TrackContextMenu::playRequested, this, &SearchPage::playRequested);
    connect(m_ctx, &TrackContextMenu::enqueueRequested, this, &SearchPage::enqueueRequested);
    connect(m_ctx, &TrackContextMenu::likeToggled, this, &SearchPage::likeToggled);

    root->addWidget(m_view, 1);

    // Debounce 200 ms — Task.md §3.5.
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(200);
    connect(m_debounce, &QTimer::timeout, this, &SearchPage::applyQuery);

    applyQuery();
}

void SearchPage::setQuery(const QString &query)
{
    m_query = query.trimmed();
    m_debounce->start();
}

void SearchPage::refresh(int currentTrackId, bool isPlaying)
{
    m_currentId = currentTrackId;
    m_playing = isPlaying;
    m_listModel->setPlaybackState(currentTrackId, isPlaying);
    // Keep current filter; only refresh data.
    m_listModel->reload();
    m_proxy->setNeedle(TextUtils::normalized(m_query));
}

QList<int> SearchPage::selectedIds() const
{
    QList<int> ids;
    for (const QModelIndex &px : m_view->selectionModel()->selectedRows()) {
        const QModelIndex src = m_proxy->mapToSource(px);
        const int id = m_listModel->trackIdAt(src.row());
        if (id > 0) ids.append(id);
    }
    return ids;
}

void SearchPage::showContext(const QPoint &globalPos)
{
    auto ids = selectedIds();
    if (!ids.isEmpty())
        m_ctx->popup(ids, globalPos);
}

void SearchPage::applyQuery()
{
    const QString needle = TextUtils::normalized(m_query);

    // Clear playlist chips.
    if (QLayout *lay = m_playlistHits->layout()) {
        QLayoutItem *it;
        while ((it = lay->takeAt(0)) != nullptr) {
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }
    }

    if (m_query.isEmpty()) {
        m_title->setText(QString());
        m_empty->setText(Lang::tr("Digite algo na busca para encontrar músicas e playlists"));
        m_empty->show();
        m_playlistHits->hide();
        m_view->hide();
        m_proxy->setNeedle(QString());
        return;
    }

    m_title->setText(QString(Lang::tr("Resultados para “%1”")).arg(m_query));
    m_proxy->setNeedle(needle);
    m_listModel->setPlaybackState(m_currentId, m_playing);

    // Playlist name hits as simple chips.
    QList<Folder> folderHits;
    for (const auto &f : m_model->folders()) {
        if (TextUtils::normalized(f.name).contains(needle))
            folderHits.append(f);
    }

    auto *lay = m_playlistHits->layout();
    if (!folderHits.isEmpty()) {
        auto *lbl = new QLabel(Lang::tr("Playlists"), m_playlistHits);
        lbl->setFont(Theme::bodyFont(12));
        lumen::design::StyleSheet::apply(lbl, QString(
            "color: %1; background: transparent; font-weight: bold;"
        ).arg(Theme::textMuted().name()));
        lay->addWidget(lbl);

        for (const auto &f : folderHits) {
            auto *btn = new QPushButton(f.name, m_playlistHits);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFont(Theme::bodyFont(13));
            lumen::design::StyleSheet::apply(btn, QString(
                "QPushButton { background: %1; color: %2; border: none; border-radius: 8px; "
                "padding: 10px 14px; text-align: left; }"
                "QPushButton:hover { background: %3; }"
            ).arg(Theme::card().name(), Theme::text().name(), Theme::cardHover().name()));
            const QString name = f.name;
            connect(btn, &QPushButton::clicked, this, [this, name]() {
                emit navigateTo(QStringLiteral("folder"), name);
            });
            lay->addWidget(btn);
        }
        m_playlistHits->show();
    } else {
        m_playlistHits->hide();
    }

    const int trackHits = m_proxy->rowCount();
    if (trackHits == 0 && folderHits.isEmpty()) {
        m_empty->setText(QString(Lang::tr(
            "Nenhum resultado para “%1”\nVerifique a escrita ou tente outras palavras"))
            .arg(m_query));
        m_empty->show();
        m_view->hide();
    } else {
        m_empty->hide();
        m_view->setVisible(trackHits > 0);
    }
}
