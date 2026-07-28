#ifndef LUMEN_DESIGN_PAINT_H
#define LUMEN_DESIGN_PAINT_H

#include <QColor>
#include <QPixmap>
#include <QString>
#include <QPainter>

namespace lumen::design::paint {

// Shared QPainter primitives + pixmap cache for cover gradients (P3 reuses this).
QPixmap gradientRect(const QColor &c1, const QColor &c2, int w, int h, int radius);
QPixmap roundedCover(const QString &path, int w, int h, int radius);

void drawGradientRect(QPainter &p, const QRect &r, const QColor &c1, const QColor &c2, int radius);

// Clear the gradient pixmap cache (call on theme density change if sizes shift).
void clearCache();

} // namespace lumen::design::paint

#endif // LUMEN_DESIGN_PAINT_H
