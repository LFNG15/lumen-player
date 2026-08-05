#ifndef LUMEN_DESIGN_STYLESHEET_H
#define LUMEN_DESIGN_STYLESHEET_H

#include "tokens.h"

#include <QPalette>
#include <QString>
#include <QWidget>

namespace lumen::design {

// Single owner of every setStyleSheet / qApp stylesheet call (gate G1).
class StyleSheet {
public:
    static QString  build(const Tokens &t);
    static QPalette palette(const Tokens &t);

    // The ONLY functions outside this translation unit that may style widgets.
    static void apply(QWidget *w, const QString &css);
    static void applyApp(const QString &css);
};

// Accent with alpha as an rgba() string for QSS fragments.
QString accentRgba(const Tokens &t, double alpha);

} // namespace lumen::design

#endif // LUMEN_DESIGN_STYLESHEET_H
