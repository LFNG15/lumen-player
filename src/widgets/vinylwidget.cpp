#include "vinylwidget.h"
#include "design/thememanager.h"

#include <QPainter>
#include <QRadialGradient>
#include <QHideEvent>
#include <QShowEvent>

using lumen::design::ThemeManager;

VinylWidget::VinylWidget(int size, QWidget *parent)
    : QWidget(parent), m_size(size)
{
    setFixedSize(m_size, m_size);
    m_gradient = { Theme::accent(), Theme::danger() };

    connect(&m_timer, &QTimer::timeout, this, [this]() {
        m_angle += stepDegrees();
        if (m_angle >= 360.0) m_angle -= 360.0;
        update();
    });

    // Theme changes invalidate the body cache (vinyl black / groove contrast).
    connect(&ThemeManager::instance(), &ThemeManager::changed, this, [this]() {
        m_bodyCache = QPixmap();
        m_bodyKey.clear();
        m_labelCache = QPixmap();
        m_labelKey.clear();
        syncTimer();
        update();
    });
}

void VinylWidget::setGradient(const Theme::GradientPair &gradient)
{
    m_gradient = gradient;
    m_labelCache = QPixmap();
    m_labelKey.clear();
    update();
}

void VinylWidget::setSpinning(bool spinning)
{
    m_wantSpin = spinning;
    syncTimer();
    update();
}

void VinylWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    // Stop the timer while hidden — idle CPU budget (Task.md P6).
    if (m_timer.isActive())
        m_timer.stop();
    m_spinning = false;
}

void VinylWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    syncTimer();
}

int VinylWidget::tickMs() const
{
    // ~33 fps base; 0 when reduce-motion is on.
    const int ms = ThemeManager::motion().d(30);
    return ms;
}

qreal VinylWidget::stepDegrees() const
{
    // Base 2° per tick; drop to 0 under reduce-motion (timer won't run then).
    return ThemeManager::motion().reduced ? 0.0 : 2.0;
}

void VinylWidget::syncTimer()
{
    const int interval = tickMs();
    const bool shouldRun = m_wantSpin && isVisible() && interval > 0;
    m_spinning = shouldRun;
    if (shouldRun) {
        if (!m_timer.isActive() || m_timer.interval() != interval)
            m_timer.start(interval);
    } else if (m_timer.isActive()) {
        m_timer.stop();
    }
}

void VinylWidget::ensureBodyCache()
{
    const QString key = QStringLiteral("vinyl|%1|%2")
                            .arg(m_size)
                            .arg(Theme::vinylBlack().name(QColor::HexRgb));
    if (!m_bodyCache.isNull() && m_bodyKey == key)
        return;

    m_bodyKey = key;
    m_bodyCache = QPixmap(m_size, m_size);
    m_bodyCache.fill(Qt::transparent);

    QPainter p(&m_bodyCache);
    p.setRenderHint(QPainter::Antialiasing);

    const QPointF center(m_size / 2.0, m_size / 2.0);
    const qreal radius = m_size / 2.0;

    // Body
    p.setBrush(Theme::vinylBlack());
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, radius - 1, radius - 1);

    // Grooves (pre-rendered once — the expensive part that used to run at 33 Hz)
    QPen groovePen;
    groovePen.setWidthF(0.5);
    for (int i = 0; i < 10; ++i) {
        const qreal r = radius * (0.25 + i * 0.065);
        groovePen.setColor(QColor(255, 255, 255, i % 2 == 0 ? 15 : 8));
        p.setPen(groovePen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center, r, r);
    }
}

void VinylWidget::ensureLabelCache()
{
    const QString key = QStringLiteral("label|%1|%2|%3")
                            .arg(m_size)
                            .arg(m_gradient.c1.name(QColor::HexRgb),
                                 m_gradient.c2.name(QColor::HexRgb));
    if (!m_labelCache.isNull() && m_labelKey == key)
        return;

    m_labelKey = key;
    m_labelCache = QPixmap(m_size, m_size);
    m_labelCache.fill(Qt::transparent);

    QPainter p(&m_labelCache);
    p.setRenderHint(QPainter::Antialiasing);

    const QPointF center(m_size / 2.0, m_size / 2.0);
    const qreal radius = m_size / 2.0;
    const qreal labelR = radius * 0.18;

    QRadialGradient labelGrad(center, labelR);
    labelGrad.setColorAt(0, m_gradient.c1);
    labelGrad.setColorAt(1, m_gradient.c2);
    p.setPen(Qt::NoPen);
    p.setBrush(labelGrad);
    p.drawEllipse(center, labelR, labelR);

    // Center hole
    p.setBrush(Theme::vinylBlack());
    p.drawEllipse(center, 2.5, 2.5);
}

QSize VinylWidget::sizeHint() const { return QSize(m_size, m_size); }
QSize VinylWidget::minimumSizeHint() const { return QSize(m_size, m_size); }

void VinylWidget::paintEvent(QPaintEvent *)
{
    ensureBodyCache();
    ensureLabelCache();

    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QPointF center(m_size / 2.0, m_size / 2.0);

    // Rotate the pre-rendered disc; label rides along (looks right for vinyl).
    p.save();
    p.translate(center);
    p.rotate(m_angle);
    p.translate(-center);
    p.drawPixmap(0, 0, m_bodyCache);
    p.drawPixmap(0, 0, m_labelCache);
    p.restore();
}
