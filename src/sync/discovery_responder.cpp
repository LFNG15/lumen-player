#include "discovery_responder.h"

#include "library_snapshot.h"   // kProtocolVersion

#include <QCoreApplication>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkDatagram>
#include <QUdpSocket>

namespace lumen::sync {

DiscoveryResponder::DiscoveryResponder(QObject *parent)
    : QObject(parent)
{
}

bool DiscoveryResponder::start(const QString &serverIdValue, const QString &name, quint16 httpPort)
{
    if (m_socket)
        return true;

    m_serverId = serverIdValue;
    m_name = name;
    m_httpPort = httpPort;

    m_socket = new QUdpSocket(this);
    // ShareAddress so a second instance (or a leftover socket) does not make the
    // whole feature fail to start.
    if (!m_socket->bind(QHostAddress::AnyIPv4, kDiscoveryPort,
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        delete m_socket;
        m_socket = nullptr;
        return false;
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &DiscoveryResponder::readPending);
    return true;
}

void DiscoveryResponder::stop()
{
    if (!m_socket)
        return;
    m_socket->close();
    m_socket->deleteLater();
    m_socket = nullptr;
}

bool DiscoveryResponder::isRunning() const
{
    return m_socket != nullptr;
}

void DiscoveryResponder::readPending()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket->receiveDatagram();
        const QByteArray payload = datagram.data();
        if (payload.isEmpty() || payload.size() > 512)
            continue;

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        const QJsonObject probe = doc.object();
        if (probe.value(QStringLiteral("lumen")).toString() != QLatin1String("discover"))
            continue;

        QJsonObject reply;
        reply[QStringLiteral("lumen")] = QStringLiteral("announce");
        reply[QStringLiteral("proto")] = kProtocolVersion;
        reply[QStringLiteral("serverId")] = m_serverId;
        reply[QStringLiteral("name")] = m_name;
        reply[QStringLiteral("port")] = m_httpPort;
        reply[QStringLiteral("appVersion")] = QCoreApplication::applicationVersion();

        // Unicast straight back to whoever asked.
        m_socket->writeDatagram(
            QJsonDocument(reply).toJson(QJsonDocument::Compact),
            datagram.senderAddress(),
            datagram.senderPort());
    }
}

} // namespace lumen::sync
