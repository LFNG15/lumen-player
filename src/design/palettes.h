#ifndef LUMEN_DESIGN_PALETTES_H
#define LUMEN_DESIGN_PALETTES_H

#include "tokens.h"
#include <QStringList>

namespace lumen::design {

struct PaletteDef {
    QString id;
    QString namePt;   // Portuguese display name (Lang key)
    Colors  dark;
    Colors  light;
};

// All six named palettes. Lumen uses Stream tokens; others keep their accents.
QList<PaletteDef> allPalettes();
PaletteDef paletteById(const QString &id);
QStringList paletteIds();

} // namespace lumen::design

#endif // LUMEN_DESIGN_PALETTES_H
