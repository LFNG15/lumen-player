#include "mediakeys.h"

#include <QAbstractEventDispatcher>
#include <QGuiApplication>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace lumen::platform {

MediaKeys::MediaKeys(QObject *parent)
    : QObject(parent)
{
}

MediaKeys::~MediaKeys()
{
    uninstall();
}

void MediaKeys::install()
{
    if (m_installed) return;
    qApp->installNativeEventFilter(this);
    m_installed = true;
}

void MediaKeys::uninstall()
{
    if (!m_installed) return;
    qApp->removeNativeEventFilter(this);
    m_installed = false;
}

bool MediaKeys::nativeEventFilter(const QByteArray &eventType, void *message,
                                  qintptr *result)
{
    Q_UNUSED(result);
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
        return false;

    const MSG *msg = static_cast<MSG *>(message);
    if (msg->message != WM_APPCOMMAND)
        return false;

    const int cmd = GET_APPCOMMAND_LPARAM(msg->lParam);
    switch (cmd) {
    case APPCOMMAND_MEDIA_PLAY_PAUSE:
        emit playPause();
        return true;
    case APPCOMMAND_MEDIA_NEXTTRACK:
        emit next();
        return true;
    case APPCOMMAND_MEDIA_PREVIOUSTRACK:
        emit previous();
        return true;
    case APPCOMMAND_MEDIA_STOP:
        emit stop();
        return true;
    case APPCOMMAND_MEDIA_PLAY:
        emit playPause();
        return true;
    case APPCOMMAND_MEDIA_PAUSE:
        emit playPause();
        return true;
    default:
        break;
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}

} // namespace lumen::platform
