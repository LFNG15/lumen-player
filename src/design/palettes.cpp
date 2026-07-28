#include "palettes.h"

namespace lumen::design {

namespace {

QColor c(const char *hex) { return QColor(QLatin1String(hex)); }

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
    o.onAccent = QColor(Qt::white);
    o.danger = danger;
    o.vinyl = vinyl;
    o.overlay = QColor(0, 0, 0, 160);
    o.selectionBar = accent;
    o.accentDim = accent.darker(120);
    return o;
}

Colors makeLight(const QColor &app, const QColor &sidebar, const QColor &card,
                 const QColor &cardHover, const QColor &input,
                 const QColor &text, const QColor &muted, const QColor &faint,
                 const QColor &border, const QColor &accent, const QColor &danger,
                 const QColor &vinyl)
{
    Colors o = makeDark(app, sidebar, card, cardHover, input, text, muted, faint,
                        border, accent, danger, vinyl);
    o.onAccent = QColor(Qt::white);
    o.overlay = QColor(255, 255, 255, 180);
    return o;
}

} // namespace

QList<PaletteDef> allPalettes()
{
    // Decision 6: only Lumen adopts Stream #ff5722. Others keep their accents.
    // Decision 1 grammar: sidebar is darker than app in dark mode, lighter in light.
    return {
        {
            QStringLiteral("lumen"), QStringLiteral("Lumen"),
            // Stream dark tokens literally
            makeDark(c("#0a0e12"), c("#070a0d"), c("#121821"), c("#1c2530"), c("#161d27"),
                     c("#eef3f6"), c("#93a1ad"), c("#5a6670"), c("#263240"),
                     c("#ff5722"), c("#ff4d4d"), c("#050506")),
            makeLight(c("#f3f6f8"), c("#e7edf1"), c("#ffffff"), c("#e4ebef"), c("#ffffff"),
                      c("#16202a"), c("#54616c"), c("#94a2ac"), c("#d2dbe1"),
                      c("#ff5722"), c("#d32f2f"), c("#1a1a1a")),
        },
        {
            QStringLiteral("warm"), QStringLiteral("Vinil Quente"),
            makeDark(c("#1a1712"), c("#14110e"), c("#2a2620"), c("#342f28"), c("#221f19"),
                     c("#f0ece4"), c("#b8b0a2"), c("#7a7266"), c("#3a352d"),
                     c("#e8a44a"), c("#d45d5d"), c("#111111")),
            makeLight(c("#f7f3ec"), c("#efe8dc"), c("#ffffff"), c("#f0e9dc"), c("#ffffff"),
                      c("#2a241c"), c("#6b6256"), c("#9a9080"), c("#d9d0c2"),
                      c("#e8a44a"), c("#c62828"), c("#1a1a1a")),
        },
        {
            QStringLiteral("ocean"), QStringLiteral("Oceano"),
            makeDark(c("#0d1520"), c("#0a1018"), c("#162338"), c("#1d2e47"), c("#111c2d"),
                     c("#e4f0f8"), c("#a0c0d8"), c("#5a7a90"), c("#1e3048"),
                     c("#4aa8e8"), c("#e85d5d"), c("#080d14")),
            makeLight(c("#eef5fa"), c("#e2edf5"), c("#ffffff"), c("#dceaf4"), c("#ffffff"),
                      c("#12202c"), c("#4a6578"), c("#7a96a8"), c("#c5d6e4"),
                      c("#4aa8e8"), c("#c62828"), c("#1a1a1a")),
        },
        {
            QStringLiteral("forest"), QStringLiteral("Floresta"),
            makeDark(c("#121a12"), c("#0e140e"), c("#1f281f"), c("#283328"), c("#182018"),
                     c("#e8f0e8"), c("#a8c0a8"), c("#6a8a6a"), c("#2a3a2a"),
                     c("#6bcf7f"), c("#d45d5d"), c("#080f08")),
            makeLight(c("#eef5ee"), c("#e2eee2"), c("#ffffff"), c("#d8ead8"), c("#ffffff"),
                      c("#152015"), c("#4a684a"), c("#7a9a7a"), c("#c5d8c5"),
                      c("#6bcf7f"), c("#c62828"), c("#1a1a1a")),
        },
        {
            QStringLiteral("purple"), QStringLiteral("Roxo Noturno"),
            makeDark(c("#15101a"), c("#100c14"), c("#261c30"), c("#30233c"), c("#1d1525"),
                     c("#f0e8f8"), c("#c0a8d8"), c("#7a6090"), c("#352545"),
                     c("#c084fc"), c("#f05d7a"), c("#0d0810")),
            makeLight(c("#f5eef8"), c("#ece3f2"), c("#ffffff"), c("#e6daf0"), c("#ffffff"),
                      c("#1e1528"), c("#6a5880"), c("#9a88b0"), c("#d8cce4"),
                      c("#c084fc"), c("#c62828"), c("#1a1a1a")),
        },
        {
            QStringLiteral("gray"), QStringLiteral("Cinza Moderno"),
            makeDark(c("#141414"), c("#0e0e0e"), c("#242424"), c("#2e2e2e"), c("#1c1c1c"),
                     c("#f5f5f5"), c("#bdbdbd"), c("#757575"), c("#333333"),
                     c("#e0e0e0"), c("#ef5350"), c("#0a0a0a")),
            makeLight(c("#f5f5f5"), c("#ebebeb"), c("#ffffff"), c("#e8e8e8"), c("#ffffff"),
                      c("#1a1a1a"), c("#616161"), c("#9e9e9e"), c("#d0d0d0"),
                      c("#424242"), c("#c62828"), c("#1a1a1a")),
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
