#include "thememanager.h"
#include "stylesheet.h"
#include "palettes.h"
#include "paint.h"

#include <QApplication>
#include <QSettings>

namespace lumen::design {

ThemeManager &ThemeManager::instance()
{
    static ThemeManager mgr;
    return mgr;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    m_tokens = buildTokens(m_paletteId, m_mode, m_density, m_hcFromLight);
}

void ThemeManager::loadFromSettings()
{
    QSettings s;
    m_paletteId = s.value(QStringLiteral("theme"), QStringLiteral("lumen")).toString();
    const QString modeStr = s.value(QStringLiteral("themeMode"), QStringLiteral("dark")).toString();
    if (modeStr == QLatin1String("light"))
        m_mode = Mode::Light;
    else if (modeStr == QLatin1String("hc") || modeStr == QLatin1String("highContrast"))
        m_mode = Mode::HighContrast;
    else
        m_mode = Mode::Dark;
    m_hcFromLight = s.value(QStringLiteral("themeHcFromLight"), false).toBool();

    const QString densStr = s.value(QStringLiteral("themeDensity"), QStringLiteral("comfortable")).toString();
    m_density = (densStr == QLatin1String("compact")) ? Density::Compact : Density::Comfortable;
    m_reduceMotion = s.value(QStringLiteral("reduceMotion"), false).toBool();
    rebuild();
}

void ThemeManager::saveToSettings() const
{
    QSettings s;
    s.setValue(QStringLiteral("theme"), m_paletteId);
    QString modeStr = QStringLiteral("dark");
    if (m_mode == Mode::Light) modeStr = QStringLiteral("light");
    else if (m_mode == Mode::HighContrast) modeStr = QStringLiteral("hc");
    s.setValue(QStringLiteral("themeMode"), modeStr);
    s.setValue(QStringLiteral("themeHcFromLight"), m_hcFromLight);
    s.setValue(QStringLiteral("themeDensity"),
               m_density == Density::Compact ? QStringLiteral("compact")
                                             : QStringLiteral("comfortable"));
    s.setValue(QStringLiteral("reduceMotion"), m_reduceMotion);
}

void ThemeManager::setPaletteId(const QString &id)
{
    if (m_paletteId == id) return;
    m_paletteId = id;
    saveToSettings();
    rebuild();
}

void ThemeManager::setMode(Mode mode)
{
    if (m_mode == mode) return;
    if (mode == Mode::HighContrast && m_mode != Mode::HighContrast)
        m_hcFromLight = (m_mode == Mode::Light);
    m_mode = mode;
    saveToSettings();
    rebuild();
}

void ThemeManager::setDensity(Density density)
{
    if (m_density == density) return;
    m_density = density;
    saveToSettings();
    rebuild();
}

void ThemeManager::setReduceMotion(bool on)
{
    if (m_reduceMotion == on) return;
    m_reduceMotion = on;
    saveToSettings();
    rebuild();
}

void ThemeManager::rebuild()
{
    m_tokens = buildTokens(m_paletteId, m_mode, m_density, m_hcFromLight);
    m_tokens.motion.reduced = m_reduceMotion;

    if (qApp) {
        qApp->setPalette(StyleSheet::palette(m_tokens));
        qApp->setFont(m_tokens.type.body);
        StyleSheet::applyApp(StyleSheet::build(m_tokens));
    }
    // Theme / density changes resize many cached surfaces — drop them so the
    // next paint rebuilds against the new tokens (P6 cover cache).
    paint::clearCache();
    emit changed();
}

} // namespace lumen::design
