// THE ONLY translation unit that may include winrt / windows headers for SMTC.
// See ContextProject.md §7.4–7.5 and Task.md P5.3.

#include "nowplaying.h"
#include "design/paint.h"

#include <QWidget>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QDebug>
#include <QMetaObject>
#include <QCoreApplication>

// After Qt headers; NOMINMAX / WIN32_LEAN_AND_MEAN are compile-wide.
#include <windows.h>
#include <unknwn.h>

// C++/WinRT
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Graphics.Imaging.h>

// Desktop interop (GetForWindow). Present in the Windows SDK.
#include <SystemMediaTransportControlsInterop.h>

namespace lumen::platform {

namespace {

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;

QString smtcCacheDir()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/smtc");
    QDir().mkpath(dir);
    return dir;
}

QString gradientThumbPath(const QColor &c1, const QColor &c2)
{
    const QString name = QStringLiteral("%1_%2.png")
                             .arg(c1.name(QColor::HexRgb).mid(1),
                                  c2.name(QColor::HexRgb).mid(1));
    const QString path = smtcCacheDir() + QLatin1Char('/') + name;
    if (!QFile::exists(path)) {
        const QPixmap pm =
            lumen::design::paint::gradientRect(c1, c2, 300, 300, 0);
        pm.save(path, "PNG");
    }
    return path;
}

winrt::hstring toHString(const QString &s)
{
    const auto ws = s.toStdWString();
    return winrt::hstring{ws.c_str(), static_cast<winrt::hstring::size_type>(ws.size())};
}

class NowPlayingWin final : public NowPlaying {
public:
    explicit NowPlayingWin(QWidget *mainWindow, QObject *parent = nullptr)
        : NowPlaying(parent)
        , m_window(mainWindow)
    {
        qRegisterMetaType<lumen::platform::TransportCommand>(
            "lumen::platform::TransportCommand");
        initSmtc();
    }

    ~NowPlayingWin() override
    {
        teardown();
    }

    bool isAvailable() const override { return m_available; }

    void setEnabled(bool enabled) override
    {
        if (!m_available) return;
        try {
            m_smtc.IsEnabled(enabled);
            m_smtc.IsPlayEnabled(enabled);
            m_smtc.IsPauseEnabled(enabled);
            m_smtc.IsNextEnabled(enabled && m_canNext);
            m_smtc.IsPreviousEnabled(enabled && m_canPrev);
        } catch (const hresult_error &e) {
            qWarning() << "SMTC setEnabled:" << e.message().c_str();
        }
    }

    void setMetadata(const NowPlayingInfo &info) override
    {
        if (!m_available) return;
        try {
            auto updater = m_smtc.DisplayUpdater();
            updater.Type(MediaPlaybackType::Music);
            auto music = updater.MusicProperties();
            music.Title(toHString(info.title));
            music.Artist(toHString(info.artist));
            if (!info.album.isEmpty())
                music.AlbumTitle(toHString(info.album));

            // Thumbnail: gradient cache on disk → RandomAccessStreamReference.
            QString thumbPath;
            if (!info.thumbnail.isNull()) {
                thumbPath = smtcCacheDir() + QStringLiteral("/custom.png");
                info.thumbnail.scaled(300, 300, Qt::KeepAspectRatioByExpanding,
                                      Qt::SmoothTransformation)
                    .save(thumbPath, "PNG");
            } else if (info.color1.isValid() && info.color2.isValid()) {
                thumbPath = gradientThumbPath(info.color1, info.color2);
            }

            if (!thumbPath.isEmpty() && QFile::exists(thumbPath)) {
                try {
                    const auto pathH = toHString(QDir::toNativeSeparators(thumbPath));
                    auto file = StorageFile::GetFileFromPathAsync(pathH).get();
                    updater.Thumbnail(RandomAccessStreamReference::CreateFromFile(file));
                } catch (const hresult_error &e) {
                    qWarning() << "SMTC thumbnail:" << e.message().c_str();
                }
            }

            updater.Update();
            m_durationMs = info.durationMs;
            updateTimeline(m_lastPosMs, m_durationMs);
        } catch (const hresult_error &e) {
            qWarning() << "SMTC setMetadata:" << e.message().c_str();
        }
    }

    void setPlaybackState(bool playing) override
    {
        if (!m_available) return;
        try {
            m_smtc.PlaybackStatus(playing ? MediaPlaybackStatus::Playing
                                          : MediaPlaybackStatus::Paused);
        } catch (const hresult_error &e) {
            qWarning() << "SMTC setPlaybackState:" << e.message().c_str();
        }
    }

    void setTimeline(qint64 posMs, qint64 durMs) override
    {
        m_lastPosMs = posMs;
        if (durMs > 0)
            m_durationMs = durMs;
        // Throttle timeline writes — SMTC doesn't need every multimedia tick.
        if (qAbs(posMs - m_lastTimelineWriteMs) < 900 && durMs == m_lastTimelineDurMs)
            return;
        m_lastTimelineWriteMs = posMs;
        m_lastTimelineDurMs = durMs;
        updateTimeline(posMs, m_durationMs);
    }

    void setCanNext(bool can) override
    {
        m_canNext = can;
        if (!m_available) return;
        try {
            m_smtc.IsNextEnabled(can);
        } catch (...) {}
    }

    void setCanPrevious(bool can) override
    {
        m_canPrev = can;
        if (!m_available) return;
        try {
            m_smtc.IsPreviousEnabled(can);
        } catch (...) {}
    }

private:
    void initSmtc()
    {
        // 1) Apartment may already be STA from Qt OLE — catch RPC_E_CHANGED_MODE.
        try {
            winrt::init_apartment(apartment_type::single_threaded);
        } catch (const hresult_error &e) {
            if (e.code() != RPC_E_CHANGED_MODE) {
                qWarning() << "SMTC init_apartment:" << e.message().c_str();
                return;
            }
            // COM already initialized in a compatible mode — continue.
        } catch (...) {
            // Proceed; Qt usually has STA already.
        }

        if (!m_window) return;
        const HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        if (!hwnd) {
            qWarning() << "SMTC: no HWND yet";
            return;
        }

        try {
            // 2) Desktop apps must use GetForWindow, not GetForCurrentView.
            auto interop = winrt::get_activation_factory<
                SystemMediaTransportControls,
                ISystemMediaTransportControlsInterop>();

            winrt::check_hresult(interop->GetForWindow(
                hwnd,
                winrt::guid_of<SystemMediaTransportControls>(),
                winrt::put_abi(m_smtc)));

            m_smtc.IsEnabled(true);
            m_smtc.IsPlayEnabled(true);
            m_smtc.IsPauseEnabled(true);
            m_smtc.IsNextEnabled(true);
            m_smtc.IsPreviousEnabled(true);
            m_smtc.IsStopEnabled(true);
            m_smtc.PlaybackStatus(MediaPlaybackStatus::Closed);

            // 3) Callbacks arrive on WinRT pool threads — marshal to GUI.
            m_buttonToken = m_smtc.ButtonPressed(
                [this](SystemMediaTransportControls const &,
                       SystemMediaTransportControlsButtonPressedEventArgs const &args) {
                    const auto btn = args.Button();
                    TransportCommand cmd = TransportCommand::Toggle;
                    switch (btn) {
                    case SystemMediaTransportControlsButton::Play:
                        cmd = TransportCommand::Play; break;
                    case SystemMediaTransportControlsButton::Pause:
                        cmd = TransportCommand::Pause; break;
                    case SystemMediaTransportControlsButton::Next:
                        cmd = TransportCommand::Next; break;
                    case SystemMediaTransportControlsButton::Previous:
                        cmd = TransportCommand::Previous; break;
                    case SystemMediaTransportControlsButton::Stop:
                        cmd = TransportCommand::Stop; break;
                    default:
                        cmd = TransportCommand::Toggle; break;
                    }
                    const TransportCommand c = cmd;
                    QMetaObject::invokeMethod(
                        this,
                        [this, c]() { emit commandReceived(c, 0); },
                        Qt::QueuedConnection);
                });

            m_seekToken = m_smtc.PlaybackPositionChangeRequested(
                [this](SystemMediaTransportControls const &,
                       PlaybackPositionChangeRequestedEventArgs const &args) {
                    const qint64 ms =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            args.RequestedPlaybackPosition())
                            .count();
                    QMetaObject::invokeMethod(
                        this,
                        [this, ms]() {
                            emit commandReceived(TransportCommand::Seek, ms);
                        },
                        Qt::QueuedConnection);
                });

            m_available = true;
            qInfo() << "SMTC: SystemMediaTransportControls ready";
        } catch (const hresult_error &e) {
            qWarning() << "SMTC init failed:" << e.message().c_str();
            m_available = false;
        } catch (...) {
            qWarning() << "SMTC init failed: unknown exception";
            m_available = false;
        }
    }

    void updateTimeline(qint64 posMs, qint64 durMs)
    {
        if (!m_available || durMs <= 0) return;
        try {
            SystemMediaTransportControlsTimelineProperties props;
            props.StartTime(TimeSpan{});
            props.MinSeekTime(TimeSpan{});
            props.Position(std::chrono::milliseconds(posMs));
            props.EndTime(std::chrono::milliseconds(durMs));
            props.MaxSeekTime(std::chrono::milliseconds(durMs));
            m_smtc.UpdateTimelineProperties(props);
        } catch (const hresult_error &e) {
            qWarning() << "SMTC timeline:" << e.message().c_str();
        }
    }

    void teardown()
    {
        if (!m_available) return;
        try {
            // 4) Revoke tokens + disable so Windows doesn't keep a ghost entry.
            if (m_buttonToken) {
                m_smtc.ButtonPressed(m_buttonToken);
                m_buttonToken = {};
            }
            if (m_seekToken) {
                m_smtc.PlaybackPositionChangeRequested(m_seekToken);
                m_seekToken = {};
            }
            m_smtc.IsEnabled(false);
            m_smtc.PlaybackStatus(MediaPlaybackStatus::Closed);
        } catch (...) {
        }
        m_available = false;
    }

    QWidget *m_window = nullptr;
    SystemMediaTransportControls m_smtc{nullptr};
    event_token m_buttonToken{};
    event_token m_seekToken{};
    bool m_available = false;
    bool m_canNext = true;
    bool m_canPrev = true;
    qint64 m_durationMs = 0;
    qint64 m_lastPosMs = 0;
    qint64 m_lastTimelineWriteMs = -10000;
    qint64 m_lastTimelineDurMs = -1;
};

} // namespace

std::unique_ptr<NowPlaying> NowPlaying::create(QWidget *mainWindow)
{
    auto impl = std::make_unique<NowPlayingWin>(mainWindow);
    if (!impl->isAvailable()) {
        // Fall back to null behavior while keeping a live object that no-ops.
        // Still return the win instance — setEnabled etc. are safe no-ops when
        // unavailable. Callers check isAvailable().
    }
    return impl;
}

} // namespace lumen::platform
