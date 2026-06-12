#ifndef TEXTUTILS_H
#define TEXTUTILS_H

#include <QString>

namespace TextUtils {

// Lowercase and strip accents so "musica" finds "Música". Used by the global
// search and the in-playlist filter.
inline QString normalized(const QString &s) {
    QString decomposed = s.normalized(QString::NormalizationForm_D).toLower();
    QString out;
    out.reserve(decomposed.size());
    for (const QChar &c : decomposed) {
        if (c.category() != QChar::Mark_NonSpacing) out.append(c);
    }
    return out;
}

}  // namespace TextUtils

#endif // TEXTUTILS_H
