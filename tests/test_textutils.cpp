#include <QtTest>
#include "textutils.h"

class TestTextUtils : public QObject {
    Q_OBJECT
private slots:
    void stripsAccents_data();
    void stripsAccents();
    void caseInsensitive();
};

void TestTextUtils::stripsAccents_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    QTest::newRow("musica") << QStringLiteral("Música") << QStringLiteral("musica");
    QTest::newRow("sao") << QStringLiteral("São") << QStringLiteral("sao");
    QTest::newRow("plain") << QStringLiteral("Hello") << QStringLiteral("hello");
    QTest::newRow("cedilla") << QStringLiteral("Ação") << QStringLiteral("acao");
}

void TestTextUtils::stripsAccents()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(TextUtils::normalized(input), expected);
}

void TestTextUtils::caseInsensitive()
{
    QVERIFY(TextUtils::normalized(QStringLiteral("ABC")).contains(
        TextUtils::normalized(QStringLiteral("abc"))));
}

QTEST_APPLESS_MAIN(TestTextUtils)
#include "test_textutils.moc"
