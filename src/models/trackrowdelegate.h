#ifndef LUMEN_TRACKROWDELEGATE_H
#define LUMEN_TRACKROWDELEGATE_H

#include <QStyledItemDelegate>

// Decision 12: on hover show [♥] [duration] [⋯]; play glyph replaces index.
// paint() and hitTest() share zoneRect() — single geometry source (Task §7.6).
class TrackRowDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    enum class Zone { None, Row, PlayGlyph, Like, More };

    explicit TrackRowDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;

    // Pure geometry — used by paint and hitTest.
    QRect zoneRect(Zone zone, const QStyleOptionViewItem &option) const;
    Zone  hitTest(const QStyleOptionViewItem &option, const QPoint &pos) const;

signals:
    void playClicked(const QModelIndex &index);
    void likeClicked(const QModelIndex &index);
    void moreClicked(const QModelIndex &index, const QPoint &globalPos);
    void rowDoubleClicked(const QModelIndex &index);

private:
    int rowHeight() const;
};

#endif // LUMEN_TRACKROWDELEGATE_H
