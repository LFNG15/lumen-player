#include "trackcontextmenu.h"
#include "lang.h"
#include "theme.h"
#include "design/stylesheet.h"

#include <QMenu>
#include <QCursor>

TrackContextMenu::TrackContextMenu(TrackModel *model, QWidget *parent)
    : QObject(parent)
    , m_model(model)
    , m_parent(parent)
{
}

QMenu *TrackContextMenu::makeStyledMenu() const
{
    auto *menu = new QMenu(m_parent);
    lumen::design::StyleSheet::apply(menu, QStringLiteral(
        "QMenu { background: %1; border: 1px solid %2; border-radius: 8px; padding: 4px; color: %3; }"
        "QMenu::item { padding: 8px 16px; border-radius: 4px; }"
        "QMenu::item:selected { background: %4; }"
        "QMenu::separator { height: 1px; background: %2; margin: 4px 0; }"
    ).arg(Theme::card().name(), Theme::border().name(), Theme::text().name(),
          Theme::cardHover().name()));
    menu->setAttribute(Qt::WA_DeleteOnClose);
    return menu;
}

void TrackContextMenu::addEnqueueAction(QMenu *menu, const QList<int> &trackIds)
{
    menu->addAction(Lang::tr("Adicionar à fila"), this, [this, trackIds]() {
        for (int id : trackIds) {
            if (Track *t = m_model->findTrack(id))
                emit enqueueRequested(*t);
        }
    });
}

void TrackContextMenu::popup(const QList<int> &trackIds, const QPoint &globalPos)
{
    if (!m_model || trackIds.isEmpty()) return;

    auto *menu = makeStyledMenu();

    const int primaryId = trackIds.first();
    Track *primary = m_model->findTrack(primaryId);

    if (primary && trackIds.size() == 1) {
        menu->addAction(Lang::tr("Tocar"), this, [this, primary]() {
            emit playRequested(*primary);
        });
        addEnqueueAction(menu, trackIds);
        menu->addSeparator();

        const bool liked = primary->liked;
        menu->addAction(liked ? Lang::tr("Descurtir") : Lang::tr("Curtir"),
                        this, [this, primaryId]() {
            emit likeToggled(primaryId);
        });

        addToPlaylistSubmenu(menu, trackIds);
        menu->addSeparator();

        menu->addAction(Lang::tr("Editar música"), this, [this, primary]() {
            emit editRequested(*primary);
        });
        menu->addAction(Lang::tr("Excluir música"), this, [this, primaryId]() {
            emit deleteRequested(primaryId);
        });
    } else {
        addEnqueueAction(menu, trackIds);
        addToPlaylistSubmenu(menu, trackIds);
        menu->addSeparator();
        menu->addAction(Lang::tr("Excluir"), this, [this, trackIds]() {
            for (int id : trackIds)
                emit deleteRequested(id);
        });
    }

    menu->popup(globalPos.isNull() ? QCursor::pos() : globalPos);
}

void TrackContextMenu::popupAddMenu(const QList<int> &trackIds, const QPoint &globalPos)
{
    if (!m_model || trackIds.isEmpty()) return;

    auto *menu = makeStyledMenu();
    addEnqueueAction(menu, trackIds);
    addToPlaylistSubmenu(menu, trackIds);
    menu->popup(globalPos.isNull() ? QCursor::pos() : globalPos);
}

void TrackContextMenu::addToPlaylistSubmenu(QMenu *menu, const QList<int> &trackIds)
{
    auto *sub = menu->addMenu(Lang::tr("Adicionar à playlist"));
    lumen::design::StyleSheet::apply(sub, menu->styleSheet());

    const auto playlists = m_model->folders();
    if (playlists.isEmpty()) {
        sub->addAction(Lang::tr("Nenhuma playlist disponível"))->setEnabled(false);
        return;
    }

    // For single track, show checkable membership.
    QList<int> memberOf;
    if (trackIds.size() == 1)
        memberOf = m_model->playlistIdsForTrack(trackIds.first());

    for (const auto &f : playlists) {
        auto *act = sub->addAction(f.name);
        if (trackIds.size() == 1) {
            act->setCheckable(true);
            act->setChecked(memberOf.contains(f.id));
        }
        const int pid = f.id;
        connect(act, &QAction::triggered, this, [this, trackIds, pid, memberOf]() {
            const bool isMember = memberOf.contains(pid);
            for (int tid : trackIds) {
                if (trackIds.size() == 1 && isMember)
                    m_model->removeTrackFromPlaylist(tid, pid);
                else
                    m_model->addTrackToPlaylist(tid, pid);
            }
            emit membershipChanged();
        });
    }
}
