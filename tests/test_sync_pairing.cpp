#include <QtTest>

#include "sync/pairing.h"

using namespace lumen::sync;

class TestSyncPairing : public QObject {
    Q_OBJECT
private slots:
    void pinIsSixDigits();
    void tokenIsHex();
    void hashIsStableAndNotThePlainToken();
    void secureEqualsMatchesSemantics();
    void guardAcceptsCorrectPin();
    void guardBurnsPinAfterMaxAttempts();
    void guardRejectsWhenDisarmed();
};

void TestSyncPairing::pinIsSixDigits()
{
    for (int i = 0; i < 50; ++i) {
        const QString pin = generatePin();
        QCOMPARE(pin.size(), 6);
        for (const QChar &c : pin)
            QVERIFY(c.isDigit());
    }
}

void TestSyncPairing::tokenIsHex()
{
    const QString token = generateToken();
    QCOMPARE(token.size(), 64); // 32 bytes hex-encoded
    for (const QChar &c : token)
        QVERIFY(c.isDigit() || (c >= QLatin1Char('a') && c <= QLatin1Char('f')));

    // Two tokens in a row must differ, or the generator is broken.
    QVERIFY(generateToken() != token);
}

void TestSyncPairing::hashIsStableAndNotThePlainToken()
{
    const QString token = QStringLiteral("abc123");
    const QString hash = hashToken(token);

    QCOMPARE(hashToken(token), hash);
    QVERIFY(hash != token);
    QCOMPARE(hash.size(), 64); // sha256 hex
    QVERIFY(hashToken(QStringLiteral("abc124")) != hash);
}

void TestSyncPairing::secureEqualsMatchesSemantics()
{
    QVERIFY(secureEquals(QStringLiteral("abc"), QStringLiteral("abc")));
    QVERIFY(!secureEquals(QStringLiteral("abc"), QStringLiteral("abd")));
    QVERIFY(!secureEquals(QStringLiteral("abc"), QStringLiteral("abcd")));
    QVERIFY(!secureEquals(QStringLiteral(""), QStringLiteral("a")));
    QVERIFY(secureEquals(QStringLiteral(""), QStringLiteral("")));
}

void TestSyncPairing::guardAcceptsCorrectPin()
{
    PinGuard guard;
    guard.reset(QStringLiteral("123456"));
    QVERIFY(guard.isArmed());

    QVERIFY(guard.verify(QStringLiteral("123456")));
    // A used PIN is burnt: it must not work twice.
    QVERIFY(!guard.isArmed());
    QVERIFY(!guard.verify(QStringLiteral("123456")));
}

void TestSyncPairing::guardBurnsPinAfterMaxAttempts()
{
    PinGuard guard;
    guard.reset(QStringLiteral("123456"));

    for (int i = 0; i < PinGuard::kMaxAttempts; ++i)
        QVERIFY(!guard.verify(QStringLiteral("000000")));

    // Exhausted: even the right PIN no longer works, which is what stops a
    // brute force over the 10^6 space.
    QVERIFY(!guard.isArmed());
    QVERIFY(!guard.verify(QStringLiteral("123456")));
}

void TestSyncPairing::guardRejectsWhenDisarmed()
{
    PinGuard guard;
    QVERIFY(!guard.isArmed());
    QVERIFY(!guard.verify(QStringLiteral("123456")));

    guard.reset(QStringLiteral("999999"));
    guard.clear();
    QVERIFY(!guard.verify(QStringLiteral("999999")));
}

QTEST_APPLESS_MAIN(TestSyncPairing)
#include "test_sync_pairing.moc"
