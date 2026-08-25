#include <QtTest>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <QTemporaryFile>

#include "sync/http_server.h"

using namespace lumen::sync;

class TestSyncHttp : public QObject {
    Q_OBJECT
private slots:
    void parsesRequestLineAndHeaders();
    void parsesQueryString();
    void parsesByteRanges();
    void servesJsonOverRealSocket();
    void servesFileWithRange();
    void unknownRouteIs404();
};

void TestSyncHttp::parsesRequestLineAndHeaders()
{
    const QByteArray head =
        "GET /v1/library HTTP/1.1\r\n"
        "Host: 192.168.0.10:45150\r\n"
        "X-Lumen-Token: deadbeef\r\n"
        "Content-Length: 0";

    HttpRequest request;
    QVERIFY(parseRequestHead(head, request));
    QCOMPARE(request.method, QStringLiteral("GET"));
    QCOMPARE(request.path, QStringLiteral("/v1/library"));
    // Header lookup is case-insensitive.
    QCOMPARE(request.header(QStringLiteral("X-Lumen-Token")), QStringLiteral("deadbeef"));
    QCOMPARE(request.header(QStringLiteral("content-length")), QStringLiteral("0"));
}

void TestSyncHttp::parsesQueryString()
{
    HttpRequest request;
    QVERIFY(parseRequestHead("GET /v1/tracks?since=42&full=true HTTP/1.1\r\n", request));
    QCOMPARE(request.path, QStringLiteral("/v1/tracks"));
    QCOMPARE(request.query.value(QStringLiteral("since")), QStringLiteral("42"));
    QCOMPARE(request.query.value(QStringLiteral("full")), QStringLiteral("true"));
}

void TestSyncHttp::parsesByteRanges()
{
    qint64 start = 0;
    qint64 end = 0;

    QVERIFY(parseByteRange(QStringLiteral("bytes=100-"), 1000, start, end));
    QCOMPARE(start, 100);
    QCOMPARE(end, 999);

    QVERIFY(parseByteRange(QStringLiteral("bytes=0-499"), 1000, start, end));
    QCOMPARE(start, 0);
    QCOMPARE(end, 499);

    // Suffix form: the last N bytes.
    QVERIFY(parseByteRange(QStringLiteral("bytes=-200"), 1000, start, end));
    QCOMPARE(start, 800);
    QCOMPARE(end, 999);

    // An end past EOF is clamped, not an error.
    QVERIFY(parseByteRange(QStringLiteral("bytes=900-5000"), 1000, start, end));
    QCOMPARE(end, 999);

    // Start past EOF is unsatisfiable.
    QVERIFY(!parseByteRange(QStringLiteral("bytes=1000-"), 1000, start, end));
    QVERIFY(!parseByteRange(QStringLiteral("items=0-10"), 1000, start, end));
}

namespace {

// Drives a real request through a real socket and returns the whole response.
//
// Client and server share this thread, so the wait has to spin the event loop:
// QAbstractSocket::waitForReadyRead() only services its own socket, which would
// starve the server and make every request time out.
QByteArray roundTrip(quint16 port, const QByteArray &request)
{
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);

    QElapsedTimer timer;
    timer.start();
    while (socket.state() != QAbstractSocket::ConnectedState && timer.elapsed() < 5000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    if (socket.state() != QAbstractSocket::ConnectedState)
        return {};

    socket.write(request);
    socket.flush();

    QByteArray response;
    timer.restart();
    // The server closes the connection when done — that is the end marker.
    while (socket.state() != QAbstractSocket::UnconnectedState && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        response.append(socket.readAll());
    }
    response.append(socket.readAll());
    return response;
}

} // namespace

void TestSyncHttp::servesJsonOverRealSocket()
{
    HttpServer server;
    server.setHandler([](const HttpRequest &request) {
        if (request.path == QLatin1String("/v1/ping"))
            return HttpResponse::json("{\"ok\":true}");
        return HttpResponse::error(404, QStringLiteral("not_found"));
    });
    QVERIFY(server.start(0));           // 0 = any free port
    const quint16 port = server.serverPort();

    const QByteArray response = roundTrip(port, "GET /v1/ping HTTP/1.1\r\nHost: x\r\n\r\n");
    QVERIFY(response.startsWith("HTTP/1.1 200 OK"));
    QVERIFY(response.contains("Content-Type: application/json"));
    QVERIFY(response.contains("{\"ok\":true}"));
}

void TestSyncHttp::servesFileWithRange()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    const QByteArray content = QByteArray("0123456789").repeated(100); // 1000 bytes
    file.write(content);
    file.flush();
    const QString path = file.fileName();

    HttpServer server;
    server.setHandler([&path](const HttpRequest &) {
        return HttpResponse::file(path, QStringLiteral("application/octet-stream"));
    });
    QVERIFY(server.start(0));
    const quint16 port = server.serverPort();

    // Whole file.
    const QByteArray full = roundTrip(port, "GET /v1/tracks/1/file HTTP/1.1\r\nHost: x\r\n\r\n");
    QVERIFY(full.startsWith("HTTP/1.1 200 OK"));
    QVERIFY(full.contains("Content-Length: 1000"));
    QVERIFY(full.contains("Accept-Ranges: bytes"));

    // Resume from byte 900 — the case the phone hits after a dropped download.
    const QByteArray partial =
        roundTrip(port, "GET /v1/tracks/1/file HTTP/1.1\r\nHost: x\r\nRange: bytes=900-\r\n\r\n");
    QVERIFY(partial.startsWith("HTTP/1.1 206 Partial Content"));
    QVERIFY(partial.contains("Content-Range: bytes 900-999/1000"));
    QVERIFY(partial.contains("Content-Length: 100"));

    const int bodyStart = partial.indexOf("\r\n\r\n") + 4;
    QCOMPARE(partial.mid(bodyStart).size(), 100);
    QCOMPARE(partial.mid(bodyStart), content.mid(900));
}

void TestSyncHttp::unknownRouteIs404()
{
    HttpServer server;
    server.setHandler([](const HttpRequest &) {
        return HttpResponse::error(404, QStringLiteral("not_found"));
    });
    QVERIFY(server.start(0));

    const QByteArray response =
        roundTrip(server.serverPort(), "GET /nope HTTP/1.1\r\nHost: x\r\n\r\n");
    QVERIFY(response.startsWith("HTTP/1.1 404 Not Found"));
    QVERIFY(response.contains("not_found"));
}

// Guiless: this exercises sockets, not widgets, so it must not need a display.
QTEST_GUILESS_MAIN(TestSyncHttp)
#include "test_sync_http.moc"
