#include "stylesheet.h"

#include <QApplication>

namespace lumen::design {

void StyleSheet::apply(QWidget *w, const QString &css)
{
    if (w)
        w->setStyleSheet(css);
}

void StyleSheet::applyApp(const QString &css)
{
    if (qApp)
        qApp->setStyleSheet(css);
}

QString accentRgba(const Tokens &t, double alpha)
{
    const QColor &a = t.color.accent;
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(a.red()).arg(a.green()).arg(a.blue()).arg(alpha);
}

QPalette StyleSheet::palette(const Tokens &t)
{
    QPalette p;
    p.setColor(QPalette::Window, t.color.app);
    p.setColor(QPalette::WindowText, t.color.text);
    p.setColor(QPalette::Base, t.color.input);
    p.setColor(QPalette::AlternateBase, t.color.card);
    p.setColor(QPalette::Text, t.color.text);
    p.setColor(QPalette::Button, t.color.card);
    p.setColor(QPalette::ButtonText, t.color.text);
    p.setColor(QPalette::Highlight, t.color.accent);
    p.setColor(QPalette::HighlightedText, t.color.onAccent);
    p.setColor(QPalette::ToolTipBase, t.color.card);
    p.setColor(QPalette::ToolTipText, t.color.text);
    p.setColor(QPalette::PlaceholderText, t.color.faint);
    p.setColor(QPalette::Link, t.color.accent);
    p.setColor(QPalette::BrightText, t.color.danger);
    return p;
}

QString StyleSheet::build(const Tokens &t)
{
    const auto &c = t.color;
    const auto &m = t.metric;
    const int bw = m.borderWidth;
    // Light mode: accent hover should deepen, not wash out further.
    const QString accentHover = (t.mode == Mode::Light)
        ? c.accent.darker(112).name()
        : c.accent.lighter(110).name();
    // Surface hover: black wash on light, white wash on dark.
    const QString surfaceHover = (t.mode == Mode::Light)
        ? QStringLiteral("rgba(0,0,0,0.06)")
        : QStringLiteral("rgba(255,255,255,0.06)");

    return QStringLiteral(R"(
        /* --- Global chrome (Lumen design system) --- */
        QWidget {
            background-color: %1;
            color: %2;
            font-family: "Segoe UI", "Noto Sans", sans-serif;
        }
        QMainWindow, QDialog {
            background-color: %1;
            color: %2;
        }

        /* Sidebar darker/lighter than content (Stream grammar) */
        QWidget#lumenSidebar {
            background-color: %3;
        }
        QWidget#lumenContent, QWidget#lumenStack, QWidget#lumenTopBar {
            background-color: transparent;
        }
        QWidget#lumenPlayerBar {
            background-color: %3;
            border-top: %14px solid %4;
        }

        /* Nav item — idle soft; hover/active solid accent + onAccent (white) */
        QPushButton#lumenNavItem {
            background: transparent;
            color: %5;
            border: none;
            border-radius: %6px;
            text-align: left;
            padding: 0 %7px;
            font-weight: 600;
            min-height: %8px;
        }
        QPushButton#lumenNavItem:hover {
            background-color: %10;
            color: %11;
        }
        QPushButton#lumenNavItem[active="true"] {
            background-color: %10;
            color: %11;
            font-weight: 700;
        }
        QPushButton#lumenNavItem[active="true"]:hover {
            background-color: %15;
            color: %11;
        }

        /* Accent / ghost / danger buttons */
        QPushButton#lumenAccentBtn {
            background-color: %10;
            color: %11;
            border: none;
            border-radius: %12px;
            padding: %13px %7px;
            font-weight: 700;
        }
        QPushButton#lumenAccentBtn:hover {
            background-color: %15;
        }
        QPushButton#lumenGhostBtn {
            background: transparent;
            color: %5;
            border: %14px solid %4;
            border-radius: %12px;
            padding: %13px %7px;
        }
        QPushButton#lumenGhostBtn:hover {
            color: %2;
            border-color: %5;
        }
        QPushButton#lumenIconBtn {
            background: transparent;
            color: %16;
            border: none;
            border-radius: %12px;
        }
        QPushButton#lumenIconBtn:hover {
            color: %2;
        }

        /* Cards */
        QWidget#lumenCard, QFrame#lumenCard {
            background-color: %9;
            border-radius: %6px;
            border: %14px solid %4;
        }
        QWidget#lumenHighlightCard {
            background-color: %9;
            border-radius: %6px;
            border: %14px solid %10;
        }

        /* Inputs */
        QLineEdit, QTextEdit, QPlainTextEdit, QComboBox {
            background-color: %17;
            color: %2;
            border: %14px solid %4;
            border-radius: %12px;
            padding: 6px 10px;
            selection-background-color: %10;
            selection-color: %11;
        }
        QLineEdit:focus, QTextEdit:focus, QComboBox:focus {
            border-color: %10;
        }
        QWidget#lumenSearchPill {
            background-color: %9;
            border: %14px solid %4;
            border-radius: 20px;
        }

        /* Menus / tooltips */
        QMenu {
            background-color: %9;
            color: %2;
            border: %14px solid %4;
            border-radius: %12px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 16px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: %10;
            color: %11;
        }
        QToolTip {
            background-color: %9;
            color: %2;
            border: %14px solid %4;
            padding: 4px 8px;
            border-radius: 4px;
        }

        /* Scrollbars */
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %4;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: %16;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QScrollBar:horizontal { height: 0px; }

        /* Lists */
        QListWidget, QListView, QTreeView {
            background: transparent;
            border: none;
            outline: none;
            color: %2;
        }
        QListWidget::item:selected, QListView::item:selected {
            background: %18;
            color: %2;
        }

        /* Micro-label (uppercase section chrome) */
        QLabel#lumenMicroLabel {
            color: %16;
            background: transparent;
            font-weight: bold;
            letter-spacing: 1px;
        }
        QLabel#lumenPageTitle {
            color: %2;
            background: transparent;
            font-weight: 900;
        }
        QLabel#lumenMuted {
            color: %16;
            background: transparent;
        }
        QLabel#lumenSoft {
            color: %5;
            background: transparent;
        }
    )")
        .arg(c.app.name(),                          // %1
             c.text.name(),                         // %2
             c.sidebar.name(),                      // %3
             c.border.name(),                       // %4
             c.muted.name(),                        // %5
             QString::number(m.radiusCard),         // %6
             QString::number(m.btnPadH),            // %7
             QString::number(m.navItemHeight),      // %8
             c.card.name(),                         // %9
             c.accent.name(),                       // %10
             c.onAccent.name(),                     // %11
             QString::number(m.radiusWidget),       // %12
             QString::number(m.btnPadV),            // %13
             QString::number(bw),                   // %14
             accentHover,                           // %15
             c.faint.name(),                        // %16
             c.input.name(),                        // %17
             accentRgba(t, 0.15));                  // %18
    Q_UNUSED(surfaceHover);
}

} // namespace lumen::design
