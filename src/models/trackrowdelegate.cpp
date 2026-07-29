#include "trackrowdelegate.h"
#include "tracklistmodel.h"
#include "design/thememanager.h"
#include "design/paint.h"
#include "design/icons.h"
#include "theme.h"

#include <QPainter>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QApplication>
#include <QAbstractItemView>
#include <QWidget>

using lumen::design::ThemeManager;
namespace Icons = lumen::design::Icons;

TrackRowDelegate::TrackRowDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

int TrackRowDelegate::rowHeight() const
{
    return ThemeManager::m().rowHeight;
}

QSize TrackRowDelegate::sizeHint(const QStyleOptionViewItem &,
                                 const QModelIndex &) const
{
    return QSize(0, rowHeight());
}

QRect TrackRowDelegate::zoneRect(Zone zone, const QStyleOptionViewItem &option) const
{
    const QRect r = option.rect.adjusted(8, 4, -8, -4);
    const int h = r.height();
    const int cy = r.center().y();

    // Right-to-left action strip: [more 28] [dur 44] [like 28]
    const int moreW = 28;
    const int durW  = 44;
    const int likeW = 28;
    const int playW = 28;
    const int swatch = 38;
    const int gap = 8;

    const QRect more(r.right() - moreW, cy - moreW / 2, moreW, moreW);
    const QRect dur (more.left() - gap - durW, cy - h / 2, durW, h);
    const QRect like(dur.left() - gap - likeW, cy - likeW / 2, likeW, likeW);
    const QRect play(r.left(), cy - playW / 2, playW, playW);
    const QRect cover(play.right() + gap, cy - swatch / 2, swatch, swatch);

    switch (zone) {
    case Zone::PlayGlyph: return play;
    case Zone::Like:      return like;
    case Zone::More:      return more;
    case Zone::Row:       return option.rect;
    case Zone::None:
    default:              return {};
    }
    Q_UNUSED(cover);
}

TrackRowDelegate::Zone TrackRowDelegate::hitTest(const QStyleOptionViewItem &option,
                                                  const QPoint &pos) const
{
    if (zoneRect(Zone::Like, option).contains(pos)) return Zone::Like;
    if (zoneRect(Zone::More, option).contains(pos)) return Zone::More;
    if (zoneRect(Zone::PlayGlyph, option).contains(pos)) return Zone::PlayGlyph;
    if (option.rect.contains(pos)) return Zone::Row;
    return Zone::None;
}

void TrackRowDelegate::paint(QPainter *p, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    const auto &c = ThemeManager::c();
    const bool hover = option.state & QStyle::State_MouseOver;
    const bool selected = option.state & QStyle::State_Selected;
    const bool current = index.data(TrackListModel::IsCurrentRole).toBool();
    const bool playing = index.data(TrackListModel::IsPlayingRole).toBool();
    const bool liked = index.data(TrackListModel::LikedRole).toBool();
    // Same chrome as sidebar nav: solid accent + onAccent (white) text.
    const bool accentRow = current || hover;

    // Background
    p->setPen(Qt::NoPen);
    if (accentRow) {
        p->setBrush(hover && !current ? c.accent.lighter(105) : c.accent);
        p->drawRoundedRect(option.rect.adjusted(2, 1, -2, -1), 8, 8);
    } else if (selected) {
        p->setBrush(c.cardHover);
        p->drawRoundedRect(option.rect.adjusted(2, 1, -2, -1), 8, 8);
    }

    // Zones
    const QRect playR = zoneRect(Zone::PlayGlyph, option);
    const QRect likeR = zoneRect(Zone::Like, option);
    const QRect moreR = zoneRect(Zone::More, option);

    // Index / play glyph
    p->setFont(ThemeManager::type().mono);
    if (hover || playing) {
        p->setFont(ThemeManager::type().icon);
        p->setPen(accentRow ? c.onAccent : c.accent);
        p->drawText(playR, Qt::AlignCenter,
                    playing ? Icons::pause() : Icons::play());
    } else {
        p->setPen(current ? c.onAccent : c.faint);
        p->drawText(playR, Qt::AlignCenter,
                    index.data(TrackListModel::IndexLabelRole).toString());
    }

    // Cover swatch
    const QColor c1 = index.data(TrackListModel::Color1Role).value<QColor>();
    const QColor c2 = index.data(TrackListModel::Color2Role).value<QColor>();
    const int swatch = 38;
    const QRect coverR(playR.right() + 8,
                       option.rect.center().y() - swatch / 2,
                       swatch, swatch);
    p->drawPixmap(coverR.topLeft(),
                  lumen::design::paint::gradientRect(c1, c2, swatch, swatch, 6));

    // Title + artist — explicit line gap (half-height split was too tight).
    const QString title = index.data(TrackListModel::TitleRole).toString();
    const QString artist = index.data(TrackListModel::ArtistRole).toString();
    const int textLeft = coverR.right() + 12;
    const int textRight = likeR.left() - 12;
    const int textW = qMax(20, textRight - textLeft);

    QFont titleFont = ThemeManager::type().body;
    titleFont.setWeight(QFont::DemiBold);
    const QFontMetrics tfm(titleFont);
    const QFontMetrics afm(ThemeManager::type().bodySm);
    constexpr int kTitleArtistGap = 3;
    const int titleH = tfm.height();
    const int artistH = afm.height();
    const int blockH = titleH + kTitleArtistGap + artistH;
    const int textTop = option.rect.center().y() - blockH / 2;

    const QColor titleColor = accentRow ? c.onAccent : c.text;
    const QColor artistColor = accentRow ? QColor(c.onAccent.red(), c.onAccent.green(),
                                                  c.onAccent.blue(), 210)
                                         : c.muted;
    const QColor iconColor = accentRow ? c.onAccent : c.faint;

    p->setFont(titleFont);
    p->setPen(titleColor);
    p->drawText(QRect(textLeft, textTop, textW, titleH),
                Qt::AlignLeft | Qt::AlignVCenter,
                tfm.elidedText(title, Qt::ElideRight, textW));

    p->setFont(ThemeManager::type().bodySm);
    p->setPen(artistColor);
    p->drawText(QRect(textLeft, textTop + titleH + kTitleArtistGap, textW, artistH),
                Qt::AlignLeft | Qt::AlignVCenter,
                afm.elidedText(artist, Qt::ElideRight, textW));

    // Like — always visible if liked; otherwise on hover
    if (liked || hover) {
        p->setFont(ThemeManager::type().icon);
        p->setPen(accentRow ? c.onAccent : (liked ? c.accent : c.faint));
        p->drawText(likeR, Qt::AlignCenter,
                    liked ? Icons::heart() : Icons::heartOutline());
    }

    // Duration — always
    const qint64 ms = index.data(TrackListModel::DurationRole).toLongLong();
    p->setFont(ThemeManager::type().mono);
    p->setPen(iconColor);
    // Duration sits between like and more — recompute from zoneRect layout.
    const QRect more = moreR;
    const QRect dur(more.left() - 8 - 44, option.rect.top(), 44, option.rect.height());
    p->drawText(dur, Qt::AlignRight | Qt::AlignVCenter, Theme::formatTime(ms));

    // "+" add affordance on hover (queue / playlist menu)
    if (hover) {
        p->setFont(ThemeManager::type().icon);
        p->setPen(iconColor);
        p->drawText(moreR, Qt::AlignCenter, Icons::add());
    }

    p->restore();
}

bool TrackRowDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index)
{
    Q_UNUSED(model);
    if (!index.isValid()) return false;

    if (event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() != Qt::LeftButton) return false;
        const Zone z = hitTest(option, me->pos());
        switch (z) {
        case Zone::PlayGlyph:
            emit playClicked(index);
            return true;
        case Zone::Like:
            emit likeClicked(index);
            return true;
        case Zone::More:
            emit moreClicked(index, me->globalPosition().toPoint());
            return true;
        case Zone::Row:
            emit playClicked(index);
            return true;
        default:
            break;
        }
    } else if (event->type() == QEvent::MouseButtonDblClick) {
        emit rowDoubleClicked(index);
        return true;
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

bool TrackRowDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view,
                                 const QStyleOptionViewItem &option,
                                 const QModelIndex &index)
{
    if (event->type() == QEvent::ToolTip) {
        const Zone z = hitTest(option, event->pos());
        QString tip;
        if (z == Zone::Like) tip = QObject::tr("Like");
        else if (z == Zone::More) tip = QObject::tr("More");
        else if (z == Zone::PlayGlyph) tip = QObject::tr("Play");
        if (!tip.isEmpty()) {
            QToolTip::showText(event->globalPos(), tip, static_cast<QWidget *>(view));
            return true;
        }
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}
