#include <QtTest>
#include <QApplication>
#include <QStyleOptionViewItem>
#include "models/trackrowdelegate.h"
#include "design/thememanager.h"

class TestTrackRowHitTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void zoneRectShared();
    void hitTestZones();
};

void TestTrackRowHitTest::initTestCase()
{
    lumen::design::ThemeManager::instance().loadFromSettings();
}

void TestTrackRowHitTest::zoneRectShared()
{
    TrackRowDelegate del;
    QStyleOptionViewItem opt;
    opt.rect = QRect(0, 0, 800, 52);
    opt.state = QStyle::State_Enabled | QStyle::State_MouseOver;

    // paint() and hitTest() must share zoneRect — rectangles non-empty.
    QVERIFY(!del.zoneRect(TrackRowDelegate::Zone::PlayGlyph, opt).isEmpty());
    QVERIFY(!del.zoneRect(TrackRowDelegate::Zone::Like, opt).isEmpty());
    QVERIFY(!del.zoneRect(TrackRowDelegate::Zone::More, opt).isEmpty());
}

void TestTrackRowHitTest::hitTestZones()
{
    TrackRowDelegate del;
    QStyleOptionViewItem opt;
    opt.rect = QRect(10, 20, 800, 52);
    opt.state = QStyle::State_Enabled | QStyle::State_MouseOver;

    const QRect like = del.zoneRect(TrackRowDelegate::Zone::Like, opt);
    const QRect more = del.zoneRect(TrackRowDelegate::Zone::More, opt);
    const QRect play = del.zoneRect(TrackRowDelegate::Zone::PlayGlyph, opt);

    QCOMPARE(del.hitTest(opt, like.center()), TrackRowDelegate::Zone::Like);
    QCOMPARE(del.hitTest(opt, more.center()), TrackRowDelegate::Zone::More);
    QCOMPARE(del.hitTest(opt, play.center()), TrackRowDelegate::Zone::PlayGlyph);
    QCOMPARE(del.hitTest(opt, QPoint(400, 46)), TrackRowDelegate::Zone::Row);
    QCOMPARE(del.hitTest(opt, QPoint(0, 0)), TrackRowDelegate::Zone::None);
}

QTEST_MAIN(TestTrackRowHitTest)
#include "test_trackrow_hittest.moc"
