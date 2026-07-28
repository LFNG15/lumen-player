#ifndef LUMEN_PLATFORM_NOWPLAYING_H
#define LUMEN_PLATFORM_NOWPLAYING_H

#include <QObject>
#include <QString>
#include <QPixmap>
#include <QColor>
#include <memory>

class QWidget;

// Platform boundary for SystemMediaTransportControls (SMTC).
// This header must NEVER include <windows.h> or <winrt/…> (ContextProject §7.4).
namespace lumen::platform {

struct NowPlayingInfo {
    QString title;
    QString artist;
    QString album;
    QPixmap thumbnail;   // optional; if null, color1/color2 gradient is used
    QColor  color1;
    QColor  color2;
    qint64  durationMs = 0;
};

enum class TransportCommand {
    Play,
    Pause,
    Toggle,
    Next,
    Previous,
    Stop,
    Seek
};

class NowPlaying : public QObject {
    Q_OBJECT
public:
    explicit NowPlaying(QObject *parent = nullptr) : QObject(parent) {}
    ~NowPlaying() override = default;

    // Factory: WinRT SMTC when available, otherwise a no-op implementation.
    // mainWindow must already have a native winId() (call after show()).
    static std::unique_ptr<NowPlaying> create(QWidget *mainWindow);

    virtual bool isAvailable() const = 0;
    virtual void setEnabled(bool enabled) = 0;
    virtual void setMetadata(const NowPlayingInfo &info) = 0;
    virtual void setPlaybackState(bool playing) = 0;
    virtual void setTimeline(qint64 posMs, qint64 durMs) = 0;
    virtual void setCanNext(bool can) = 0;
    virtual void setCanPrevious(bool can) = 0;

signals:
    // Emitted on the GUI thread (callbacks are marshalled with QueuedConnection).
    void commandReceived(lumen::platform::TransportCommand command, qint64 argMs = 0);
};

} // namespace lumen::platform

Q_DECLARE_METATYPE(lumen::platform::TransportCommand)

#endif // LUMEN_PLATFORM_NOWPLAYING_H
