#ifndef THEME_H
#define THEME_H

// Shim over lumen::design::ThemeManager (P1 Pass A).
// All color/font accessors read live tokens so a single ThemeManager::rebuild()
// updates every call site. Do NOT style widgets from here — use
// lumen::design::StyleSheet::apply (the only allowed owner of that API).

#include "design/thememanager.h"
#include "design/stylesheet.h"
#include "design/palettes.h"
#include "design/paint.h"

#include <QString>
#include <QColor>
#include <QFont>
#include <QRandomGenerator>
#include <QList>
#include <QPixmap>

namespace Theme {

struct ThemeData {
    QString id;
    QString name;
    QColor bg, surface, card, cardHover;
    QColor accent, accentDim;
    QColor text, textSoft, textMuted, border, danger, vinylBlack;
};

inline ThemeData fromTokens(const QString &id, const QString &name,
                            const lumen::design::Colors &c)
{
    ThemeData t;
    t.id = id;
    t.name = name;
    t.bg = c.app;
    t.surface = c.sidebar;       // Stream grammar: sidebar token
    t.card = c.card;
    t.cardHover = c.cardHover;
    t.accent = c.accent;
    t.accentDim = c.accentDim;
    t.text = c.text;
    t.textSoft = c.muted;
    t.textMuted = c.faint;
    t.border = c.border;
    t.danger = c.danger;
    t.vinylBlack = c.vinyl;
    return t;
}

inline ThemeData themeById(const QString &id)
{
    const auto pal = lumen::design::paletteById(id);
    // Report the dark base for the theme picker swatches.
    return fromTokens(pal.id, pal.namePt, pal.dark);
}

inline QList<ThemeData> allThemes()
{
    QList<ThemeData> out;
    for (const auto &p : lumen::design::allPalettes())
        out.append(fromTokens(p.id, p.namePt, p.dark));
    return out;
}

inline ThemeData activeTheme()
{
    const auto &tok = lumen::design::ThemeManager::t();
    const auto pal = lumen::design::paletteById(tok.paletteId);
    return fromTokens(pal.id, pal.namePt, tok.color);
}

// Legacy API: setActiveTheme maps to ThemeManager palette change (live).
inline void setActiveTheme(const ThemeData &t)
{
    lumen::design::ThemeManager::instance().setPaletteId(t.id);
}

inline QColor bg()          { return lumen::design::ThemeManager::c().app; }
inline QColor surface()     { return lumen::design::ThemeManager::c().sidebar; }
inline QColor card()        { return lumen::design::ThemeManager::c().card; }
inline QColor cardHover()   { return lumen::design::ThemeManager::c().cardHover; }
inline QColor accent()      { return lumen::design::ThemeManager::c().accent; }
inline QColor accentDim()   { return lumen::design::ThemeManager::c().accentDim; }
inline QColor text()        { return lumen::design::ThemeManager::c().text; }
inline QColor textSoft()    { return lumen::design::ThemeManager::c().muted; }
inline QColor textMuted()   { return lumen::design::ThemeManager::c().faint; }
inline QColor border()      { return lumen::design::ThemeManager::c().border; }
inline QColor danger()      { return lumen::design::ThemeManager::c().danger; }
inline QColor vinylBlack()  { return lumen::design::ThemeManager::c().vinyl; }
// Text/icon color on solid accent fills (not bg() — bg is light in light mode).
inline QColor onAccent()    { return lumen::design::ThemeManager::c().onAccent; }

inline bool isLightMode()
{
    return lumen::design::ThemeManager::instance().mode()
        == lumen::design::Mode::Light;
}

inline bool isLightSurface()
{
    return lumen::design::ThemeManager::c().app.lightness() > 128;
}

// Mode-aware wash for hover/press on surfaces (white wash fails on light UI).
inline QString hoverBg(double alpha = 0.06)
{
    if (isLightSurface())
        return QStringLiteral("rgba(0,0,0,%1)").arg(alpha);
    return QStringLiteral("rgba(255,255,255,%1)").arg(alpha);
}

// Accent button hover: deepen on light, lift on dark.
inline QColor accentHover()
{
    return isLightSurface() ? accent().darker(112) : accent().lighter(110);
}

inline QString accentRgba(double alpha)
{
    return lumen::design::accentRgba(lumen::design::ThemeManager::t(), alpha);
}

struct GradientPair {
    QColor c1, c2;
};

inline QList<GradientPair> palettes()
{
    return {
        { QColor(QStringLiteral("#e8a44a")), QColor(QStringLiteral("#d45d5d")) },
        { QColor(QStringLiteral("#6b8f71")), QColor(QStringLiteral("#2c3e50")) },
        { QColor(QStringLiteral("#c9a959")), QColor(QStringLiteral("#8b5e3c")) },
        { QColor(QStringLiteral("#5d7b93")), QColor(QStringLiteral("#2b2d42")) },
        { QColor(QStringLiteral("#d4a373")), QColor(QStringLiteral("#9c6644")) },
        { QColor(QStringLiteral("#b07156")), QColor(QStringLiteral("#4a3228")) },
        { QColor(QStringLiteral("#a3b18a")), QColor(QStringLiteral("#344e41")) },
        { QColor(QStringLiteral("#dda15e")), QColor(QStringLiteral("#bc6c25")) },
        { QColor(QStringLiteral("#8ecae6")), QColor(QStringLiteral("#023047")) },
        { QColor(QStringLiteral("#cdb4db")), QColor(QStringLiteral("#5a189a")) },
    };
}

inline GradientPair randomPalette()
{
    auto p = palettes();
    return p[QRandomGenerator::global()->bounded(p.size())];
}

inline QFont titleFont(int size = 28)
{
    QFont f = lumen::design::ThemeManager::type().title;
    if (size > 0) f.setPointSize(size);
    return f;
}

inline QFont bodyFont(int size = 14)
{
    QFont f = lumen::design::ThemeManager::type().body;
    if (size > 0) f.setPointSize(size);
    return f;
}

inline QFont monoFont(int size = 11)
{
    QFont f = lumen::design::ThemeManager::type().mono;
    if (size > 0) f.setPointSize(size);
    return f;
}

inline QFont iconFont(int size = 14)
{
    QFont f = lumen::design::ThemeManager::type().icon;
    if (size > 0) f.setPointSize(size);
    return f;
}

inline QString globalStyleSheet()
{
    return lumen::design::StyleSheet::build(lumen::design::ThemeManager::t());
}

inline QPixmap roundedCover(const QString &path, int w, int h, int radius)
{
    return lumen::design::paint::roundedCover(path, w, h, radius);
}

inline QString formatTime(qint64 ms)
{
    int totalSec = static_cast<int>(ms / 1000);
    int min = totalSec / 60;
    int sec = totalSec % 60;
    return QStringLiteral("%1:%2").arg(min).arg(sec, 2, 10, QLatin1Char('0'));
}

}  // namespace Theme

#endif // THEME_H
