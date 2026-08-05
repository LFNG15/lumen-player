#ifndef LUMEN_PLATFORM_MEDIAKEYS_H
#define LUMEN_PLATFORM_MEDIAKEYS_H

#include <QAbstractNativeEventFilter>
#include <QObject>

// Fallback media-key handling via WM_APPCOMMAND when SMTC is unavailable.
// Does NOT use RegisterHotKey (conflicts with SMTC — Task.md §5.4 / §6).
namespace lumen::platform {

class MediaKeys : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit MediaKeys(QObject *parent = nullptr);
    ~MediaKeys() override;

    void install();
    void uninstall();

    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           qintptr *result) override;

signals:
    void playPause();
    void next();
    void previous();
    void stop();

private:
    bool m_installed = false;
};

} // namespace lumen::platform

#endif // LUMEN_PLATFORM_MEDIAKEYS_H
