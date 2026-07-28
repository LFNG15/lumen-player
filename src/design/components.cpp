#include "components.h"
#include "thememanager.h"
#include "icons.h"
#include "stylesheet.h"

#include <QVBoxLayout>
#include <QStyle>


namespace lumen::design {

QPushButton *accentButton(const QString &text, QWidget *parent)
{
    auto *b = new QPushButton(text, parent);
    b->setObjectName(QStringLiteral("lumenAccentBtn"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFont(ThemeManager::type().body);
    return b;
}

QPushButton *ghostButton(const QString &text, QWidget *parent)
{
    auto *b = new QPushButton(text, parent);
    b->setObjectName(QStringLiteral("lumenGhostBtn"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFont(ThemeManager::type().body);
    return b;
}

QPushButton *iconButton(const QString &glyph, QWidget *parent)
{
    auto *b = new QPushButton(glyph, parent);
    b->setObjectName(QStringLiteral("lumenIconBtn"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFont(ThemeManager::type().icon);
    b->setFlat(true);
    return b;
}

QPushButton *navItem(const QString &text, const QString &glyph, QWidget *parent)
{
    auto *b = new QPushButton(QStringLiteral("  %1  %2").arg(glyph, text), parent);
    b->setObjectName(QStringLiteral("lumenNavItem"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFont(ThemeManager::type().body);
    b->setProperty("active", false);
    b->setCheckable(false);
    return b;
}

void setNavItemActive(QPushButton *btn, bool active)
{
    if (!btn) return;
    btn->setProperty("active", active);
    // Force QSS to re-evaluate dynamic property.
    btn->style()->unpolish(btn);
    btn->style()->polish(btn);
    btn->update();
}

QFrame *card(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setObjectName(QStringLiteral("lumenCard"));
    f->setFrameShape(QFrame::NoFrame);
    return f;
}

QFrame *highlightCard(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setObjectName(QStringLiteral("lumenHighlightCard"));
    f->setFrameShape(QFrame::NoFrame);
    return f;
}

QLabel *microLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text.toUpper(), parent);
    l->setObjectName(QStringLiteral("lumenMicroLabel"));
    l->setFont(ThemeManager::type().micro);
    return l;
}

QLabel *pageTitle(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("lumenPageTitle"));
    l->setFont(ThemeManager::type().title);
    return l;
}

QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("lumenMuted"));
    l->setFont(ThemeManager::type().bodySm);
    return l;
}

QWidget *pageHeader(const QString &title, QWidget *parent)
{
    auto *w = new QWidget(parent);
    w->setObjectName(QStringLiteral("lumenPageHeader"));
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, ThemeManager::m().spacing);
    lay->setSpacing(ThemeManager::m().spacingSm);

    auto *bullet = new QLabel(Icons::bullet(), w);
    bullet->setFont(ThemeManager::type().title);
    StyleSheet::apply(bullet, QStringLiteral("color: %1; background: transparent;")
                                  .arg(ThemeManager::c().accent.name()));

    auto *titleLbl = pageTitle(title, w);
    lay->addWidget(bullet, 0, Qt::AlignVCenter);
    lay->addWidget(titleLbl, 0, Qt::AlignVCenter);
    lay->addStretch();
    return w;
}

QWidget *sectionHeader(const QString &label, QWidget *parent)
{
    auto *w = new QWidget(parent);
    w->setObjectName(QStringLiteral("lumenSectionHeader"));
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, ThemeManager::m().spacing, 0, ThemeManager::m().spacingSm);
    lay->addWidget(microLabel(label, w));
    lay->addStretch();
    return w;
}

} // namespace lumen::design
