#include "paint.h"

#include <QCache>
#include <QPainterPath>
#include <QMutex>
#include <QMutexLocker>
#include <QFileInfo>
#include <QDateTime>

namespace lumen::design::paint {

namespace {

// Cap at 32 MB of pixmap payload (cost unit = KB). Task.md P6.
QCache<QString, QPixmap> &cache()
{
    static QCache<QString, QPixmap> c(32 * 1024);
    return c;
}

QMutex &cacheMutex()
{
    static QMutex m;
    return m;
}

int costKb(int w, int h)
{
    // 4 bytes/pixel ARGB → KB, at least 1 so the entry is tracked.
    return qMax(1, (w * h * 4) / 1024);
}

} // namespace

void clearCache()
{
    QMutexLocker lock(&cacheMutex());
    cache().clear();
}

QPixmap gradientRect(const QColor &c1, const QColor &c2, int w, int h, int radius)
{
    // Key format: c1|c2|w|h|r  (Task.md P6)
    const QString key = QStringLiteral("c1|%1|c2|%2|%3|%4|%5")
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
        cache().insert(key, new QPixmap(pm), costKb(w, h));
    }
    return pm;
}

void drawGradientRect(QPainter &p, const QRect &r, const QColor &c1, const QColor &c2, int radius)
{
    p.drawPixmap(r.topLeft(), gradientRect(c1, c2, r.width(), r.height(), radius));
}

QPixmap roundedCover(const QString &path, int w, int h, int radius)
{
    // Key format: path|mtime|w|h|r  (Task.md P6) — invalidate on file change.
    const qint64 mtime = QFileInfo(path).lastModified().toMSecsSinceEpoch();
    const QString key = QStringLiteral("path|%1|%2|%3|%4|%5")
                            .arg(path).arg(mtime).arg(w).arg(h).arg(radius);
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
        cache().insert(key, new QPixmap(result), costKb(w, h));
    }
    return result;
}

} // namespace lumen::design::paint
