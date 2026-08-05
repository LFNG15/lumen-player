#ifndef COVERWIDGET_H
#define COVERWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QGridLayout>
#include "theme.h"
#include "design/stylesheet.h"
#include "trackmodel.h"

namespace CoverWidget {

// Builds a square playlist cover following the app's hierarchy:
// cover image > 2x2 mosaic of track gradients (4+ tracks) > playlist gradient.
// Mouse-transparent so it can sit inside clickable rows/cards.
inline QWidget *playlistCover(const Folder &folder, const QList<Track> &tracks,
                              int size, int radius, QWidget *parent = nullptr) {
    auto *cover = new QWidget(parent);
    cover->setFixedSize(size, size);
    cover->setAttribute(Qt::WA_TransparentForMouseEvents);

    QPixmap pix = folder.coverImage.isEmpty()
        ? QPixmap() : Theme::roundedCover(folder.coverImage, size, size, radius);
    if (!pix.isNull()) {
        auto *img = new QLabel(cover);
        img->setGeometry(0, 0, size, size);
        img->setPixmap(pix);
        lumen::design::StyleSheet::apply(img, "background: transparent;");
        return cover;
    }

    if (tracks.size() >= 4) {
        auto *grid = new QGridLayout(cover);
        grid->setSpacing(1);
        grid->setContentsMargins(0, 0, 0, 0);
        for (int i = 0; i < 4; ++i) {
            auto *cell = new QWidget();
            lumen::design::StyleSheet::apply(cell, QString(
                "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: %3px;")
                .arg(tracks[i].cover.c1.name(), tracks[i].cover.c2.name())
                .arg(qMax(2, radius / 2)));
            grid->addWidget(cell, i / 2, i % 2);
        }
    } else {
        lumen::design::StyleSheet::apply(cover, QString(
            "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: %3px;")
            .arg(folder.cover.c1.name(), folder.cover.c2.name())
            .arg(radius));
    }
    return cover;
}

}  // namespace CoverWidget

#endif // COVERWIDGET_H
