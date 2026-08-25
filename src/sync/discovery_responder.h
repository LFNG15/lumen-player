#ifndef LUMEN_SYNC_DISCOVERY_RESPONDER_H
#define LUMEN_SYNC_DISCOVERY_RESPONDER_H

#include <QObject>
#include <QString>

class QUdpSocket;

namespace lumen::sync {

inline constexpr quint16 kDefaultHttpPort = 45150;
inline constexpr quint16 kDiscoveryPort   = 45151;

// Answers the phone's discovery probe.
//
// This is NOT mDNS. Qt has no mDNS implementation, and the alternatives each
// cost more than they are worth here: qmdnsengine is a new dependency with
// multicast quirks on Windows, and the Windows DNS-SD API ties us to a verbose
// C API. Probe/response over UDP gives the same user-visible behaviour — the
// desktop announces itself, the phone lists what it finds — with about forty
// lines and no new dependency. On Android, sending a broadcast and reading the
// unicast reply also avoids needing a MulticastLock.
//
// The phone broadcasts {"lumen":"discover","proto":1} to 255.255.255.255:45151;
// we reply unicast to the sender with the server identity and HTTP port.
class DiscoveryResponder : public QObject {
    Q_OBJECT
public:
    explicit DiscoveryResponder(QObject *parent = nullptr);

    bool start(const QString &serverId, const QString &name, quint16 httpPort);
    void stop();

    bool isRunning() const;

private:
    void readPending();

    QUdpSocket *m_socket = nullptr;
    QString     m_serverId;
    QString     m_name;
    quint16     m_httpPort = kDefaultHttpPort;
};

} // namespace lumen::sync

#endif // LUMEN_SYNC_DISCOVERY_RESPONDER_H
