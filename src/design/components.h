#ifndef LUMEN_DESIGN_COMPONENTS_H
#define LUMEN_DESIGN_COMPONENTS_H

#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QString>
#include <QHBoxLayout>

namespace lumen::design {

// Factories that set objectName / properties so the global QSS styles them.
// Prefer these over ad-hoc setStyleSheet in pages.

QPushButton *accentButton(const QString &text, QWidget *parent = nullptr);
QPushButton *ghostButton(const QString &text, QWidget *parent = nullptr);
QPushButton *iconButton(const QString &glyph, QWidget *parent = nullptr);
QPushButton *navItem(const QString &text, const QString &glyph, QWidget *parent = nullptr);

QFrame *card(QWidget *parent = nullptr);
QFrame *highlightCard(QWidget *parent = nullptr);

QLabel *microLabel(const QString &text, QWidget *parent = nullptr);
QLabel *pageTitle(const QString &text, QWidget *parent = nullptr);
QLabel *mutedLabel(const QString &text, QWidget *parent = nullptr);

// Page header: ◆ accent bullet + title (+ optional trailing widgets via layout).
QWidget *pageHeader(const QString &title, QWidget *parent = nullptr);
QWidget *sectionHeader(const QString &label, QWidget *parent = nullptr);

void setNavItemActive(QPushButton *btn, bool active);

} // namespace lumen::design

#endif // LUMEN_DESIGN_COMPONENTS_H
