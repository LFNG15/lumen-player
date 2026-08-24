#ifndef LUMEN_SYNC_HTTP_SERVER_H
#define LUMEN_SYNC_HTTP_SERVER_H

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTcpServer>

#include <functional>

class QTcpSocket;

namespace lumen::sync {

// Minimal HTTP/1.1 server on top of QTcpServer.
//
// Qt is here without the QHttpServer add-on (it is an optional component of the
// online installer and is NOT part of the kit this project builds against), so
// depending on it would mean changing both the local toolchain and CI. We only
// have to serve one client we control, so a small hand-rolled server is the
// cheaper, testable option: request line + headers, body by Content-Length,
// responses with Content-Length or a streamed file, and `Range: bytes=N-` for
// resumable downloads.
//
// Every response closes the connection (`Connection: close`); OkHttp simply
// reopens, and it keeps connection state out of the picture.
struct HttpRequest {
    QString method;
    QString path;                       // without query string
    QHash<QString, QString> query;
    QHash<QString, QString> headers;    // keys lowercased
    QByteArray body;

    QString header(const QString &name) const { return headers.value(name.toLower()); }
};

struct HttpResponse {
    int status = 200;
    QString contentType = QStringLiteral("application/json; charset=utf-8");
    QByteArray body;
    QHash<QString, QString> extraHeaders;

    // When set, the body is streamed from this file instead of `body`.
    QString filePath;

    static HttpResponse json(const QByteArray &payload, int status = 200);
    static HttpResponse error(int status, const QString &code);
    static HttpResponse file(const QString &path, const QString &contentType);
};

class HttpServer : public QTcpServer {
    Q_OBJECT
public:
    using Handler = std::function<HttpResponse(const HttpRequest &)>;

    explicit HttpServer(QObject *parent = nullptr);

    void setHandler(Handler handler) { m_handler = std::move(handler); }

    // Listens on all interfaces so the phone can reach it over Wi-Fi.
    bool start(quint16 port);
    void stop();

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    // Parse state for one connection, kept until the socket dies.
    struct ConnectionState {
        QByteArray  buffer;
        bool        headParsed = false;
        HttpRequest request;
        qint64      expectedBody = 0;
    };

    void handleReadyRead(QTcpSocket *socket);
    void dispatch(QTcpSocket *socket, const HttpRequest &request);
    void sendResponse(QTcpSocket *socket, const HttpRequest &request, const HttpResponse &response);
    void streamFile(QTcpSocket *socket, const HttpRequest &request, const HttpResponse &response);

    Handler m_handler;
    QHash<QTcpSocket *, ConnectionState> m_states;
};

// Exposed for unit tests: parses a raw request head into method/path/query/headers.
bool parseRequestHead(const QByteArray &head, HttpRequest &out);

// Parses `Range: bytes=START-[END]`. Returns false when absent or unusable.
bool parseByteRange(const QString &value, qint64 fileSize, qint64 &start, qint64 &end);

} // namespace lumen::sync

#endif // LUMEN_SYNC_HTTP_SERVER_H
