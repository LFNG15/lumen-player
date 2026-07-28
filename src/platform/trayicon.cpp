#include "trayicon.h"
#include "playbackengine.h"
#include "lang.h"
#include "theme.h"
#include "design/stylesheet.h"

#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QIcon>

namespace lumen::platform {

TrayIcon::TrayIcon(PlaybackEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QIcon(QStringLiteral(":/icon.png")));
    m_tray->setToolTip(QStringLiteral("Lumen Music"));

    m_menu = new QMenu();
    lumen::design::StyleSheet::apply(m_menu, QStringLiteral(
        "QMenu { background: %1; color: %2; border: 1px solid %3; }"
        "QMenu::item:selected { background: %4; }"
    ).arg(Theme::card().name(), Theme::text().name(), Theme::border().name(),
          Theme::cardHover().name()));

    rebuildMenu();
    m_tray->setContextMenu(m_menu);

    connect(m_tray, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);

    if (m_engine) {
        connect(m_engine, &PlaybackEngine::playingChanged,
                this, &TrayIcon::syncPlaying);
        connect(m_engine, &PlaybackEngine::trackChanged, this, [this](int) {
            const Track t = m_engine->currentTrack();
            if (t.id != 0)
                m_tray->setToolTip(QStringLiteral("%1 — %2").arg(t.title, t.artist));
            else
                m_tray->setToolTip(QStringLiteral("Lumen Music"));
        });
    }
}

TrayIcon::~TrayIcon()
{
    if (m_tray)
        m_tray->hide();
    delete m_menu;
    m_menu = nullptr;
}

bool TrayIcon::isAvailable() const
{
    return m_tray != nullptr;
}

void TrayIcon::show()
{
    if (m_tray)
        m_tray->show();
}

void TrayIcon::hide()
{
    if (m_tray)
        m_tray->hide();
}

void TrayIcon::rebuildMenu()
{
    if (!m_menu) return;
    m_menu->clear();

    auto *showAct = m_menu->addAction(Lang::tr("Mostrar Lumen Music"));
    connect(showAct, &QAction::triggered, this, &TrayIcon::showMainWindow);

    m_menu->addSeparator();

    auto *playAct = m_menu->addAction(Lang::tr("Play / Pause"));
    connect(playAct, &QAction::triggered, this, [this]() {
        if (m_engine) m_engine->togglePlay();
    });
    auto *nextAct = m_menu->addAction(Lang::tr("Próxima"));
    connect(nextAct, &QAction::triggered, this, [this]() {
        if (m_engine) m_engine->next();
    });
    auto *prevAct = m_menu->addAction(Lang::tr("Anterior"));
    connect(prevAct, &QAction::triggered, this, [this]() {
        if (m_engine) m_engine->prev();
    });

    m_menu->addSeparator();
    auto *quitAct = m_menu->addAction(Lang::tr("Sair"));
    connect(quitAct, &QAction::triggered, this, &TrayIcon::quitRequested);
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
        emit showMainWindow();
}

void TrayIcon::syncPlaying(bool /*playing*/)
{
    // Could swap icon; keep single app icon for now.
}

} // namespace lumen::platform
