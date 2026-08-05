#include "nowplaying.h"

namespace lumen::platform {

namespace {

class NowPlayingNull final : public NowPlaying {
public:
    explicit NowPlayingNull(QObject *parent = nullptr) : NowPlaying(parent) {}

    bool isAvailable() const override { return false; }
    void setEnabled(bool) override {}
    void setMetadata(const NowPlayingInfo &) override {}
    void setPlaybackState(bool) override {}
    void setTimeline(qint64, qint64) override {}
    void setCanNext(bool) override {}
    void setCanPrevious(bool) override {}
};

} // namespace

std::unique_ptr<NowPlaying> NowPlaying::create(QWidget * /*mainWindow*/)
{
    return std::make_unique<NowPlayingNull>();
}

} // namespace lumen::platform
