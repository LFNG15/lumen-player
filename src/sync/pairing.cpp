#include "pairing.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

namespace lumen::sync {

QString generatePin()
{
    const quint32 value = QRandomGenerator::system()->bounded(1000000u);
    return QStringLiteral("%1").arg(value, 6, 10, QLatin1Char('0'));
}

QString generateToken()
{
    QByteArray raw(32, Qt::Uninitialized);
    QRandomGenerator::system()->generate(raw.begin(), raw.end());
    return QString::fromLatin1(raw.toHex());
}

QString hashToken(const QString &token)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(token.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool secureEquals(const QString &a, const QString &b)
{
    const QByteArray ba = a.toUtf8();
    const QByteArray bb = b.toUtf8();
    // Compare the lengths into the accumulator too, so an early return never
    // reveals whether the length or the content differed.
    int diff = ba.size() ^ bb.size();
    const int n = qMax(ba.size(), bb.size());
    for (int i = 0; i < n; ++i) {
        const uchar ca = i < ba.size() ? static_cast<uchar>(ba[i]) : 0;
        const uchar cb = i < bb.size() ? static_cast<uchar>(bb[i]) : 0;
        diff |= (ca ^ cb);
    }
    return diff == 0;
}

void PinGuard::reset(const QString &pin)
{
    m_pin = pin;
    m_attempts = 0;
    m_armed = !pin.isEmpty();
}

void PinGuard::clear()
{
    m_pin.clear();
    m_attempts = 0;
    m_armed = false;
}

bool PinGuard::verify(const QString &candidate)
{
    if (!m_armed)
        return false;

    ++m_attempts;
    const bool ok = secureEquals(m_pin, candidate);
    if (ok || m_attempts >= kMaxAttempts)
        clear(); // burn the PIN on success and on exhaustion alike
    return ok;
}

} // namespace lumen::sync
