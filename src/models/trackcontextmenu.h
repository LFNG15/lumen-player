#ifndef LUMEN_TRACKCONTEXTMENU_H
#define LUMEN_TRACKCONTEXTMENU_H

#include <QObject>
#include <QList>
#include "trackmodel.h"

class QWidget;
class QMenu;

// Shared context menu for track rows (right-click / ⋯ / Menu key).
class TrackContextMenu : public QObject {
    Q_OBJECT
public:
    explicit TrackContextMenu(TrackModel *model, QWidget *parent = nullptr);

    // Build and popup at globalPos for the given track ids (multi-select).
    void popup(const QList<int> &trackIds, const QPoint &globalPos);

    // Lightweight "+" menu: only "add to queue" and "add to playlist".
    void popupAddMenu(const QList<int> &trackIds, const QPoint &globalPos);

signals:
    void playRequested(const Track &track);
    void enqueueRequested(const Track &track);
    void editRequested(const Track &track);
    void deleteRequested(int id);
    void likeToggled(int id);
    void membershipChanged();

private:
    QMenu *makeStyledMenu() const;
    void addToPlaylistSubmenu(QMenu *menu, const QList<int> &trackIds);
    void addEnqueueAction(QMenu *menu, const QList<int> &trackIds);

    TrackModel *m_model = nullptr;
    QWidget *m_parent = nullptr;
};

#endif // LUMEN_TRACKCONTEXTMENU_H
