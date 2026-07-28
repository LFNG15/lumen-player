#include "paint.h"

#include <QCache>
#include <QPainterPath>
#include <QMutex>
#include <QMutexLocker>

namespace lumen::design::paint {

namespace {

QCache<QString, QPixmap> &cache()
{
    static QCache<QString, QPixmap> c(32 * 1024); // ~32 MB cost units
    return c;
}

QMutex &cacheMutex()
{
    static QMutex m;
    return m;
}

} // namespace

void clearCache()
{
    QMutexLocker lock(&cacheMutex());
    cache().clear();
}

QPixmap gradientRect(const QColor &c1, const QColor &c2, int w, int h, int radius)
{
    const QString key = QStringLiteral("g|%1|%2|%3|%4|%5")
                            .arg(c1.name(QColor::HexRgb), c2.name(QColor::HexRgb))
                            .arg(w).arg(h).arg(radius);
    {
        QMutexLocker lock(&cacheMutex());
        if (QPixmap *hit = cache().object(key))
            return *hit;
    }

    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient g(0, 0, w, h);
    g.setColorAt(0, c1);
    g.setColorAt(1, c2);
    QPainterPath path;
    path.addRoundedRect(0, 0, w, h, radius, radius);
    p.fillPath(path, g);

    {
        QMutexLocker lock(&cacheMutex());
        cache().insert(key, new QPixmap(pm), (w * h * 4) / 1024);
    }
    return pm;
}

void drawGradientRect(QPainter &p, const QRect &r, const QColor &c1, const QColor &c2, int radius)
{
    p.drawPixmap(r.topLeft(), gradientRect(c1, c2, r.width(), r.height(), radius));
}

QPixmap roundedCover(const QString &path, int w, int h, int radius)
{
    const QString key = QStringLiteral("f|%1|%2|%3|%4").arg(path).arg(w).arg(h).arg(radius);
    {
        QMutexLocker lock(&cacheMutex());
        if (QPixmap *hit = cache().object(key))
            return *hit;
    }

    QPixmap src(path);
    if (src.isNull())
        return QPixmap();

    QPixmap scaled = src.scaled(w, h, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap result(w, h);
    result.fill(Qt::transparent);
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(0, 0, w, h, radius, radius);
    p.setClipPath(clip);
    const int x = (scaled.width()  - w) / 2;
    const int y = (scaled.height() - h) / 2;
    p.drawPixmap(-x, -y, scaled);

    {
        QMutexLocker lock(&cacheMutex());
        cache().insert(key, new QPixmap(result), (w * h * 4) / 1024);
    }
    return result;
}

} // namespace lumen::design::paint
