#ifndef REORDERABLELIST_H
#define REORDERABLELIST_H

#include <QListWidget>
#include <QDropEvent>

// A QListWidget that emits the source/target rows on an internal drag-drop
// instead of moving items itself (which would discard setItemWidget widgets).
// The owner persists the new order and rebuilds the list.
class ReorderableList : public QListWidget {
    Q_OBJECT
public:
    explicit ReorderableList(QWidget *parent = nullptr) : QListWidget(parent) {}

signals:
    void moveRequested(int from, int to);

protected:
    void dropEvent(QDropEvent *event) override {
        int from = currentRow();
        if (from < 0) { event->ignore(); return; }

        int to;
        QModelIndex idx = indexAt(event->position().toPoint());
        if (!idx.isValid()) {
            to = count() - 1;                       // dropped past the last row
        } else {
            to = idx.row();
            if (dropIndicatorPosition() == QAbstractItemView::BelowItem) to += 1;
            if (from < to) to -= 1;                 // removing 'from' shifts indices
        }
        to = qBound(0, to, count() - 1);
        // IgnoreAction: we handle the reorder ourselves via moveRequested.
        // Without this, accept()+MoveAction makes QAbstractItemView::startDrag
        // call clearOrRemove() and delete the source row (masked today only
        // because FolderDetailPage::refresh() rebuilds the whole list).
        event->setDropAction(Qt::IgnoreAction);
        event->accept();
        if (to != from) emit moveRequested(from, to);
    }
};

#endif // REORDERABLELIST_H
