#ifndef LUMEN_PLATFORM_TRAYICON_H
#define LUMEN_PLATFORM_TRAYICON_H

#include <QObject>
#include <QSystemTrayIcon>

class QMenu;
class PlaybackEngine;

// System tray icon — pure Qt, no platform #ifdefs (Task.md P5.5).
namespace lumen::platform {

class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(PlaybackEngine *engine, QObject *parent = nullptr);
    ~TrayIcon() override;

    bool isAvailable() const;
    void show();
    void hide();

signals:
    void showMainWindow();
    void quitRequested();

private:
    void rebuildMenu();
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void syncPlaying(bool playing);

    PlaybackEngine *m_engine = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
};

} // namespace lumen::platform

#endif // LUMEN_PLATFORM_TRAYICON_H
