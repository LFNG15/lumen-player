#include "tokens.h"
#include "palettes.h"

#include <QtMath>
#include <algorithm>

namespace lumen::design {

namespace {

double relativeLuminance(const QColor &c)
{
    auto lin = [](int ch) {
        double s = ch / 255.0;
        return s <= 0.03928 ? s / 12.92 : qPow((s + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin(c.red()) + 0.7152 * lin(c.green()) + 0.0722 * lin(c.blue());
}

double contrastRatio(const QColor &a, const QColor &b)
{
    double l1 = relativeLuminance(a);
    double l2 = relativeLuminance(b);
    if (l1 < l2) std::swap(l1, l2);
    return (l1 + 0.05) / (l2 + 0.05);
}

// Push `fg` toward pure white/black until contrast vs `bg` reaches AA (4.5).
QColor ensureContrast(const QColor &fg, const QColor &bg, double minRatio = 4.5)
{
    if (contrastRatio(fg, bg) >= minRatio)
        return fg;
    const bool darkBg = relativeLuminance(bg) < 0.5;
    QColor target = darkBg ? QColor(Qt::white) : QColor(Qt::black);
    QColor cur = fg;
    for (int i = 0; i < 12; ++i) {
        cur.setRed  ((cur.red()   * 2 + target.red())   / 3);
        cur.setGreen((cur.green() * 2 + target.green()) / 3);
        cur.setBlue ((cur.blue()  * 2 + target.blue())  / 3);
        if (contrastRatio(cur, bg) >= minRatio)
            return cur;
    }
    return target;
}

Typography buildTypography(Density density)
{
    Typography t;
    const int titleSz = density == Density::Compact ? 22 : 28;
    const int bodySz  = density == Density::Compact ? 12 : 14;
    const int smSz    = density == Density::Compact ? 11 : 12;
    const int microSz = density == Density::Compact ? 9  : 10;

    t.display = QFont(QStringLiteral("Segoe UI"), titleSz + 4);
    t.display.setWeight(QFont::Black);
    t.title = QFont(QStringLiteral("Segoe UI"), titleSz);
    t.title.setWeight(QFont::Black);
    t.body = QFont(QStringLiteral("Segoe UI"), bodySz);
    t.body.setWeight(QFont::Medium);
    t.bodySm = QFont(QStringLiteral("Segoe UI"), smSz);
    t.bodySm.setWeight(QFont::Medium);
    t.micro = QFont(QStringLiteral("Segoe UI"), microSz);
    t.micro.setWeight(QFont::DemiBold);
    t.micro.setCapitalization(QFont::AllUppercase);
    t.mono = QFont(QStringLiteral("Consolas"), density == Density::Compact ? 10 : 11);
    t.mono.setStyleHint(QFont::Monospace);
    t.icon = QFont(QStringLiteral("Segoe MDL2 Assets"), bodySz);
    return t;
}

Metrics buildMetrics(Density density, Mode mode)
{
    Metrics m;
    if (density == Density::Compact) {
        m.spacing = 7;
        m.spacingSm = 4;
        m.spacingLg = 12;
        m.windowMargin = 12;
        m.btnPadH = 10;
        m.btnPadV = 5;
        m.rowHeight = 44;
        m.navItemHeight = 38;
    } else {
        m.spacing = 10;
        m.spacingSm = 6;
        m.spacingLg = 18;
        m.windowMargin = 18;
        m.btnPadH = 14;
        m.btnPadV = 8;
        // Title+artist + modest wash inset; keep rows compact between tracks.
        m.rowHeight = 54;
        m.navItemHeight = 46;
    }
    m.radiusCard = 10;
    m.radiusWidget = 8;
    m.narrowBreakpoint = 470;
    m.borderWidth = (mode == Mode::HighContrast) ? 2 : 1;
    return m;
}

} // namespace

Colors deriveHighContrast(const Colors &base)
{
    Colors hc = base;
    const bool dark = relativeLuminance(base.app) < 0.5;
    hc.text   = dark ? QColor(Qt::white) : QColor(Qt::black);
    hc.border = hc.text;
    hc.muted  = ensureContrast(base.muted, base.app, 4.5);
    hc.faint  = ensureContrast(base.faint, base.app, 3.0);
    hc.accent = ensureContrast(base.accent, base.app, 4.5);
    hc.danger = ensureContrast(base.danger, base.app, 4.5);
    hc.onAccent = ensureContrast(QColor(Qt::white), hc.accent, 4.5);
    // Flatten translucent fills to solids for HC.
    hc.cardHover = dark ? base.card.lighter(130) : base.card.darker(110);
    hc.selectionBar = hc.accent;
    return hc;
}

Tokens buildTokens(const QString &paletteId, Mode mode, Density density)
{
    const PaletteDef pal = paletteById(paletteId);
    Tokens t;
    t.paletteId = pal.id;
    t.mode = mode;
    t.density = density;

    Colors base = (mode == Mode::Light) ? pal.light : pal.dark;
    // HighContrast is derived from the light/dark base of the same family.
    if (mode == Mode::HighContrast) {
        // Prefer dark base for HC-dark feel when system is dark; callers pick.
        // Convention: HighContrast derives from dark unless palette was light.
        base = deriveHighContrast(pal.dark);
    } else if (mode == Mode::Light) {
        base = pal.light;
        // Harden light text roles against white cards / pale app bg.
        base.text  = ensureContrast(base.text,  base.card, 4.5);
        base.muted = ensureContrast(base.muted, base.card, 4.5);
        base.faint = ensureContrast(base.faint, base.card, 3.0);
        base.accent = ensureContrast(base.accent, base.card, 3.0);
        base.onAccent = ensureContrast(base.onAccent, base.accent, 4.5);
        base.border = ensureContrast(base.border, base.card, 1.4);
        // If border still too soft vs card, deepen slightly.
        if (contrastRatio(base.border, base.card) < 1.4)
            base.border = base.border.darker(120);
    } else {
        base = pal.dark;
    }
    t.color = base;
    t.metric = buildMetrics(density, mode);
    t.type = buildTypography(density);
    return t;
}

} // namespace lumen::design
