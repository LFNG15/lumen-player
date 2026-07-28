#ifndef LUMEN_DESIGN_ICONS_H
#define LUMEN_DESIGN_ICONS_H

#include <QString>

// Named Segoe MDL2 Assets glyphs — kills magic PUA literals scattered in UI.
namespace lumen::design::Icons {

inline QString play()       { return QStringLiteral(u"\uE768"); }
inline QString pause()      { return QStringLiteral(u"\uE769"); }
inline QString next()       { return QStringLiteral(u"\uE893"); }
inline QString previous()   { return QStringLiteral(u"\uE892"); }
inline QString shuffle()    { return QStringLiteral(u"\uE8B1"); }
inline QString repeat()     { return QStringLiteral(u"\uE8EE"); }
inline QString volume()     { return QStringLiteral(u"\uE767"); }
inline QString volumeMute() { return QStringLiteral(u"\uE74F"); }
inline QString heart()      { return QStringLiteral(u"\uE00B"); }
inline QString heartFill()  { return QStringLiteral(u"\uE0A5"); }
inline QString more()       { return QStringLiteral(u"\uE712"); }
inline QString search()     { return QStringLiteral(u"\uE721"); }
inline QString home()       { return QStringLiteral(u"\uE10F"); }
inline QString add()        { return QStringLiteral(u"\uE109"); }
inline QString folder()     { return QStringLiteral(u"\uE188"); }
inline QString settings()   { return QStringLiteral(u"\uE713"); }
inline QString edit()       { return QStringLiteral(u"\uE70F"); }
inline QString trash()      { return QStringLiteral(u"\uE74D"); }
inline QString queue()      { return QStringLiteral(u"\uE8FD"); }
inline QString back()       { return QStringLiteral(u"\uE72B"); }
inline QString chevron()    { return QStringLiteral(u"\uE76C"); }
inline QString sort()       { return QStringLiteral(u"\uE8CB"); }
inline QString list()       { return QStringLiteral(u"\uE8FD"); }
inline QString grid()       { return QStringLiteral(u"\uE80A"); }
inline QString theme()      { return QStringLiteral(u"\uE790"); }
// Page-title accent bullet (Stream grammar uses ◆).
inline QString bullet()     { return QStringLiteral(u"\u25C6"); }

} // namespace lumen::design::Icons

#endif // LUMEN_DESIGN_ICONS_H
