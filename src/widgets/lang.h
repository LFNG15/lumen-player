#ifndef LANG_H
#define LANG_H

// Thin facade over lumen::design::LanguageManager (P1.5).
// Existing Lang::tr / setActiveLang call sites keep working; language now
// switches live without restarting the app.

#include "design/i18n.h"

#include <QString>
#include <functional>

class QLabel;
class QAbstractButton;
class QLineEdit;
class QWidget;

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

inline void bindText(QLabel *w, const QString &pt)
{
    lumen::design::LanguageManager::instance().bindText(w, pt);
}
inline void bindText(QAbstractButton *w, const QString &pt)
{
    lumen::design::LanguageManager::instance().bindText(w, pt);
}
inline void bindPlaceholder(QLineEdit *w, const QString &pt)
{
    lumen::design::LanguageManager::instance().bindPlaceholder(w, pt);
}
inline void bindToolTip(QWidget *w, const QString &pt)
{
    lumen::design::LanguageManager::instance().bindToolTip(w, pt);
}
inline void bind(QObject *owner, std::function<void()> reapply)
{
    lumen::design::LanguageManager::instance().bind(owner, std::move(reapply));
}

} // namespace Lang

#endif // LANG_H
