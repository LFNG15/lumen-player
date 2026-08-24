#ifndef LUMEN_SYNC_PAIRING_H
#define LUMEN_SYNC_PAIRING_H

#include <QString>

// Pairing primitives for the LAN sync server.
//
// Threat model: the server only listens on the local network and only while the
// user has the sync dialog open (or explicitly left it on). A 6-digit PIN shown
// on the desktop authorises one device; from then on the device carries a
// 32-byte token. Only the token's SHA-256 is persisted, so a leaked vinil.db
// cannot be turned into a working credential.
namespace lumen::sync {

// Six digits, zero-padded. Uses QRandomGenerator::system() — the PIN guards
// access to the whole library, so it must not come from a seedable generator.
QString generatePin();

// 32 random bytes, hex-encoded.
QString generateToken();

QString hashToken(const QString &token);

// Constant-time comparison: a plain == leaks the position of the first
// mismatching byte through timing.
bool secureEquals(const QString &a, const QString &b);

// Tracks PIN attempts for one dialog session. Five wrong tries burn the PIN so
// an attacker on the LAN cannot walk the 10^6 space.
class PinGuard {
public:
    static constexpr int kMaxAttempts = 5;

    void reset(const QString &pin);
    void clear();

    bool isArmed() const { return m_armed; }
    QString pin() const { return m_pin; }
    int attemptsLeft() const { return kMaxAttempts - m_attempts; }

    // Consumes one attempt. Returns true only for the right PIN while armed.
    bool verify(const QString &candidate);

private:
    QString m_pin;
    int     m_attempts = 0;
    bool    m_armed = false;
};

} // namespace lumen::sync

#endif // LUMEN_SYNC_PAIRING_H
