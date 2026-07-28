#ifndef LANG_H
#define LANG_H

// Thin facade over lumen::design::LanguageManager (P1.5).
// Existing Lang::tr / setActiveLang call sites keep working; language now
// switches live without restarting the app.

#include "design/i18n.h"

#include <QString>

namespace Lang {

inline QString &activeLang()
{
    // Mutable ref for rare call sites that write activeLang() = ...;
    // Prefer setActiveLang / LanguageManager::setLang.
    static QString cache = lumen::design::LanguageManager::instance().lang();
    cache = lumen::design::LanguageManager::instance().lang();
    return cache;
}

inline void setActiveLang(const QString &lang)
{
    lumen::design::LanguageManager::instance().setLang(lang);
}

inline bool isEnglish()
{
    return lumen::design::LanguageManager::instance().isEnglish();
}

inline QString tr(const QString &pt)
{
    return lumen::design::LanguageManager::instance().tr(pt);
}

} // namespace Lang

#endif // LANG_H
