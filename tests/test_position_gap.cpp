#include <QtTest>
#include "database/position_gap.h"

class TestPositionGap : public QObject {
    Q_OBJECT
private slots:
    void nextAfterEmpty();
    void nextAfterMax();
    void betweenOpenGap();
    void betweenClosedGap();
    void renormalise();
};

void TestPositionGap::nextAfterEmpty()
{
    QCOMPARE(lumen::position::nextAfter(0), 1024);
    QCOMPARE(lumen::position::nextAfter(-1), 1024);
}

void TestPositionGap::nextAfterMax()
{
    QCOMPARE(lumen::position::nextAfter(1024), 2048);
    QCOMPARE(lumen::position::nextAfter(3072), 4096);
}

void TestPositionGap::betweenOpenGap()
{
    const qint64 mid = lumen::position::between(1024, 2048);
    QVERIFY(mid > 1024);
    QVERIFY(mid < 2048);
    QCOMPARE(mid, 1536);
}

void TestPositionGap::betweenClosedGap()
{
    QCOMPARE(lumen::position::between(10, 11), -1);
    QVERIFY(lumen::position::gapClosed(10, 11));
    QVERIFY(!lumen::position::gapClosed(1024, 2048));
}

void TestPositionGap::renormalise()
{
    const auto pos = lumen::position::renormalise({7, 3, 9});
    QCOMPARE(pos.size(), 3);
    QCOMPARE(pos[0].first, 7);
    QCOMPARE(pos[0].second, 1024);
    QCOMPARE(pos[1].second, 2048);
    QCOMPARE(pos[2].second, 3072);
}

QTEST_APPLESS_MAIN(TestPositionGap)
#include "test_position_gap.moc"
