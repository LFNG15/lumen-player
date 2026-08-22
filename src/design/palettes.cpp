#include "palettes.h"

#include <QtMath>

namespace lumen::design {

namespace {

QColor c(const char *hex) { return QColor(QLatin1String(hex)); }

double lum(const QColor &col)
{
    auto lin = [](int ch) {
        const double s = ch / 255.0;
        return s <= 0.03928 ? s / 12.92 : qPow((s + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin(col.red()) + 0.7152 * lin(col.green()) + 0.0722 * lin(col.blue());
}

// Text on accent fill: white on deep accents, near-black on pastel/light accents.
QColor pickOnAccent(const QColor &accent)
{
    return lum(accent) > 0.55 ? QColor(QStringLiteral("#141414"))
                              : QColor(Qt::white);
}

Colors makeDark(const QColor &app, const QColor &sidebar, const QColor &card,
                const QColor &cardHover, const QColor &input,
                const QColor &text, const QColor &muted, const QColor &faint,
                const QColor &border, const QColor &accent, const QColor &danger,
                const QColor &vinyl)
{
    Colors o;
    o.app = app;
    o.sidebar = sidebar;
    o.card = card;
    o.cardHover = cardHover;
    o.input = input;
    o.text = text;
    o.muted = muted;
    o.faint = faint;
    o.border = border;
    o.accent = accent;
    o.onAccent = pickOnAccent(accent);
    o.danger = danger;
    o.vinyl = vinyl;
    o.overlay = QColor(0, 0, 0, 160);
    o.selectionBar = accent;
    o.accentDim = accent.darker(115);
    return o;
}

// Light mode: stronger borders/muted text, tinted surfaces (not pure monochrome),
// onAccent computed for real contrast on filled buttons.
Colors makeLight(const QColor &app, const QColor &sidebar, const QColor &card,
                 const QColor &cardHover, const QColor &input,
                 const QColor &text, const QColor &muted, const QColor &faint,
                 const QColor &border, const QColor &accent, const QColor &danger,
                 const QColor &vinyl)
{
    Colors o;
    o.app = app;
    o.sidebar = sidebar;
    o.card = card;
    o.cardHover = cardHover;
    o.input = input;
    o.text = text;
    o.muted = muted;
    o.faint = faint;
    o.border = border;
    o.accent = accent;
    o.onAccent = pickOnAccent(accent);
    o.danger = danger;
    o.vinyl = vinyl;
    // Dim scrims use black, not white (white overlay vanishes on light UI).
    o.overlay = QColor(15, 20, 30, 120);
    o.selectionBar = accent;
    // Slightly deeper accent for secondary/hover chrome on light surfaces.
    o.accentDim = accent.darker(112);
    return o;
}

} // namespace

QList<PaletteDef> allPalettes()
{
    // Decision 6: only Lumen adopts Stream #ff5722. Others keep their accents.
    // Light: app (page) lighter than sidebar; cards white with visible borders;
    // muted/faint must stay readable on white (AA-ish); accents keep hue identity.
    return {
        {
            QStringLiteral("lumen"), QStringLiteral("Lumen"),
            makeDark(c("#0a0e12"), c("#070a0d"), c("#121821"), c("#1c2530"), c("#161d27"),
                     c("#eef3f6"), c("#93a1ad"), c("#5a6670"), c("#263240"),
                     c("#ff5722"), c("#ff4d4d"), c("#050506")),
            // Light: cool paper + coral accent (not washed-out orange-on-white)
            makeLight(c("#f0f4f7"), c("#dce4ea"), c("#ffffff"), c("#d0dce6"), c("#ffffff"),
                      c("#0f1720"), c("#3d4d5c"), c("#5c6b78"), c("#9aafbd"),
                      c("#e64a19"), c("#c62828"), c("#1a1a1a")),
        },
        {
            QStringLiteral("warm"), QStringLiteral("Vinil Quente"),
            makeDark(c("#1a1712"), c("#14110e"), c("#2a2620"), c("#342f28"), c("#221f19"),
                     c("#f0ece4"), c("#b8b0a2"), c("#7a7266"), c("#3a352d"),
                     c("#e8a44a"), c("#d45d5d"), c("#111111")),
            makeLight(c("#faf6ef"), c("#f0e6d6"), c("#ffffff"), c("#ebe0ce"), c("#fffdf9"),
                      c("#1f1810"), c("#5c4f3e"), c("#7a6b56"), c("#cbbda8"),
                      c("#c47a1a"), c("#b71c1c"), c("#1a1a1a")),
        },
        {
            QStringLiteral("ocean"), QStringLiteral("Oceano"),
            makeDark(c("#0d1520"), c("#0a1018"), c("#162338"), c("#1d2e47"), c("#111c2d"),
                     c("#e4f0f8"), c("#a0c0d8"), c("#5a7a90"), c("#1e3048"),
                     c("#4aa8e8"), c("#e85d5d"), c("#080d14")),
            makeLight(c("#eef5fb"), c("#dceaf4"), c("#ffffff"), c("#cfe0ee"), c("#ffffff"),
                      c("#0c1824"), c("#35566c"), c("#4f738c"), c("#a8c0d4"),
                      c("#0277bd"), c("#c62828"), c("#1a1a1a")),
        },
        {
            QStringLiteral("forest"), QStringLiteral("Floresta"),
            makeDark(c("#121a12"), c("#0e140e"), c("#1f281f"), c("#283328"), c("#182018"),
                     c("#e8f0e8"), c("#a8c0a8"), c("#6a8a6a"), c("#2a3a2a"),
                     c("#6bcf7f"), c("#d45d5d"), c("#080f08")),
            makeLight(c("#eef6ee"), c("#dceadc"), c("#ffffff"), c("#cfe0cf"), c("#ffffff"),
                      c("#101a10"), c("#3a5a3a"), c("#557555"), c("#a8c4a8"),
                      c("#2e7d32"), c("#c62828"), c("#1a1a1a")),
        },
        {
            QStringLiteral("purple"), QStringLiteral("Roxo Noturno"),
            makeDark(c("#15101a"), c("#100c14"), c("#261c30"), c("#30233c"), c("#1d1525"),
                     c("#f0e8f8"), c("#c0a8d8"), c("#7a6090"), c("#352545"),
                     c("#c084fc"), c("#f05d7a"), c("#0d0810")),
            makeLight(c("#f6f0fa"), c("#ebe0f2"), c("#ffffff"), c("#e0d2ec"), c("#ffffff"),
                      c("#16101f"), c("#4a3a62"), c("#6a5888"), c("#c4b3d6"),
                      c("#7b2cbf"), c("#c2185b"), c("#1a1a1a")),
        },
        {
            QStringLiteral("gray"), QStringLiteral("Cinza Moderno"),
            makeDark(c("#141414"), c("#0e0e0e"), c("#242424"), c("#2e2e2e"), c("#1c1c1c"),
                     c("#f5f5f5"), c("#bdbdbd"), c("#757575"), c("#333333"),
                     c("#e0e0e0"), c("#ef5350"), c("#0a0a0a")),
            // Light gray was monochrome (gray accent on gray paper). Use a clear
            // blue accent so interactive chrome stays readable and distinct.
            makeLight(c("#f4f5f7"), c("#e8eaee"), c("#ffffff"), c("#dfe2e8"), c("#ffffff"),
                      c("#12141a"), c("#3d4450"), c("#5c6570"), c("#b8bec8"),
                      c("#2563eb"), c("#c62828"), c("#1a1a1a")),
        },
    };
}

PaletteDef paletteById(const QString &id)
{
    for (const auto &p : allPalettes())
        if (p.id == id) return p;
    return allPalettes().first();
}

QStringList paletteIds()
{
    QStringList ids;
    for (const auto &p : allPalettes())
        ids << p.id;
    return ids;
}

} // namespace lumen::design
