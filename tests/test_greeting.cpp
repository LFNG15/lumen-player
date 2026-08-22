#include <QtTest>
#include "greeting.h"
#include "design/i18n.h"

class TestGreeting : public QObject {
    Q_OBJECT
private slots:
    void mapsEveryHour();
    void lateNightCrossesMidnight();
    void dawnStartsAtOne();
    void variantIndexWraps();
    void everyPhraseHasEnglish();
};

void TestGreeting::mapsEveryHour()
{
    // 0 → noite alta; 1–4 madrugada; 5–11 manhã; 12–17 tarde; 18–21 noite; 22–23 noite alta
    const int expected[24] = {
        4, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
        2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4
    };
    for (int h = 0; h < 24; ++h)
        QCOMPARE(greetingBand(h), expected[h]);
}

void TestGreeting::lateNightCrossesMidnight()
{
    QCOMPARE(greetingBand(22), 4);
    QCOMPARE(greetingBand(23), 4);
    QCOMPARE(greetingBand(0), 4);
    QVERIFY(greetingPtFor(22, 0).contains(QStringLiteral("madrugada"))
            || greetingPtFor(22, 0).contains(QStringLiteral("tarde"))
            || greetingPtFor(22, 0).contains(QStringLiteral("noite")));
}

void TestGreeting::dawnStartsAtOne()
{
    QCOMPARE(greetingBand(1), 0);
    QVERIFY(greetingPtFor(1, 0).contains(QStringLiteral("madrugada"))
            || greetingPtFor(1, 0).contains(QStringLiteral("Madrugando")));
}

void TestGreeting::variantIndexWraps()
{
    const QString a = greetingPtFor(8, 0);
    const QString b = greetingPtFor(8, greetingVariantCount(8));
    QCOMPARE(a, b);
    QCOMPARE(greetingPtFor(8, -1), greetingPtFor(8, greetingVariantCount(8) - 1));
}

void TestGreeting::everyPhraseHasEnglish()
{
    const QStringList all = allGreetingPt();
    QVERIFY(all.size() >= 15);
    for (const QString &pt : all) {
        QVERIFY2(lumen::design::englishDictContains(pt),
                 qPrintable(QStringLiteral("missing enDict entry: %1").arg(pt)));
    }
}

QTEST_APPLESS_MAIN(TestGreeting)
#include "test_greeting.moc"
