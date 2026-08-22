#include <QtTest>
#include "tools/ytdlp_bootstrap.h"

class TestYtDlpVersion : public QObject {
    Q_OBJECT
private slots:
    void olderThanPin();
    void equalToPin();
    void newerThanPin();
    void malformedIsOutdated();
    void extraTextAroundVersion();
};

void TestYtDlpVersion::olderThanPin()
{
    QCOMPARE(lumen::tools::compareYtDlpVersion(
                 QStringLiteral("2025.10.14"), QStringLiteral("2026.08.19")),
             -1);
}

void TestYtDlpVersion::equalToPin()
{
    QCOMPARE(lumen::tools::compareYtDlpVersion(
                 QStringLiteral("2026.08.19"), QStringLiteral("2026.08.19")),
             0);
}

void TestYtDlpVersion::newerThanPin()
{
    QCOMPARE(lumen::tools::compareYtDlpVersion(
                 QStringLiteral("2026.12.01"), QStringLiteral("2026.08.19")),
             1);
}

void TestYtDlpVersion::malformedIsOutdated()
{
    QCOMPARE(lumen::tools::compareYtDlpVersion(
                 QStringLiteral("not-a-version"), QStringLiteral("2026.08.19")),
             -1);
    QCOMPARE(lumen::tools::compareYtDlpVersion(
                 QString(), QStringLiteral("2026.08.19")),
             -1);
}

void TestYtDlpVersion::extraTextAroundVersion()
{
    QCOMPARE(lumen::tools::compareYtDlpVersion(
                 QStringLiteral("2026.08.19 [debug]"), QStringLiteral("2026.08.19")),
             0);
}

QTEST_APPLESS_MAIN(TestYtDlpVersion)
#include "test_ytdlp_version.moc"
