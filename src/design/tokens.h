#ifndef LUMEN_DESIGN_TOKENS_H
#define LUMEN_DESIGN_TOKENS_H

#include <QColor>
#include <QFont>
#include <QString>

namespace lumen::design {

enum class Mode    { Dark, Light, HighContrast };
enum class Density { Comfortable, Compact };

struct Colors {
    QColor app;
    QColor sidebar;
    QColor card;
    QColor cardHover;
    QColor input;
    QColor text;
    QColor muted;
    QColor faint;
    QColor border;
    QColor accent;
    QColor onAccent;
    QColor danger;
    QColor vinyl;
    QColor overlay;
    QColor selectionBar;
    // Legacy aliases used by Theme:: shim / older call sites.
    QColor accentDim;
};

struct Metrics {
    int spacing    = 10;
    int spacingSm  = 6;
    int spacingLg  = 18;
    int windowMargin = 18;
    int btnPadH    = 14;
    int btnPadV    = 8;
    int radiusCard = 10;
    int radiusWidget = 8;
    int rowHeight  = 52;
    int navItemHeight = 46;
    int narrowBreakpoint = 470;
    int borderWidth = 1;
};

struct Typography {
    QFont display;
    QFont title;
    QFont body;
    QFont bodySm;
    QFont micro;
    QFont mono;
    QFont icon;
};

struct Motion {
    int fast   = 120;
    int normal = 160;
    int slow   = 200;
    bool reduced = false;
    // Single choke point for motion budgets (Task.md P6 / decision 10).
    // Returns 0 when reduce-motion is on so timers/animations become no-ops.
    int d(int ms) const { return reduced ? 0 : ms; }
    bool shouldAnimate() const { return !reduced; }
};

struct Tokens {
    QString paletteId;
    Mode mode = Mode::Dark;
    Density density = Density::Comfortable;
    Colors color;
    Metrics metric;
    Typography type;
    Motion motion;
};

// Derive WCAG-oriented high-contrast from a base light/dark palette.
Colors deriveHighContrast(const Colors &base);

// Build full token set for a palette × mode × density combination.
Tokens buildTokens(const QString &paletteId, Mode mode, Density density);

} // namespace lumen::design

#endif // LUMEN_DESIGN_TOKENS_H
