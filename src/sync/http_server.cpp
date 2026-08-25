#include "http_server.h"

#include <QFile>
#include <QFileInfo>
#include <QTcpSocket>
#include <QUrl>

namespace lumen::sync {

namespace {

constexpr int kMaxHeadBytes = 16 * 1024;      // a request head this big is abuse
constexpr qint64 kMaxBodyBytes = 8 * 1024 * 1024;  // the push payload is small
constexpr qint64 kChunkBytes = 64 * 1024;

const char *reasonPhrase(int status)
{
    switch (status) {
    case 200: return "OK";
    case 206: return "Partial Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 416: return "Range Not Satisfiable";
    case 500: return "Internal Server Error";
    default:  return "OK";
    }
}

} // namespace

HttpResponse HttpResponse::json(const QByteArray &payload, int status)
{
    HttpResponse r;
    r.status = status;
    r.body = payload;
    return r;
}

HttpResponse HttpResponse::error(int status, const QString &code)
{
    HttpResponse r;
    r.status = status;
    r.body = QStringLiteral("{\"error\":\"%1\"}").arg(code).toUtf8();
    return r;
}

HttpResponse HttpResponse::file(const QString &path, const QString &contentType)
{
    HttpResponse r;
    r.filePath = path;
    r.contentType = contentType;
    return r;
}

bool parseRequestHead(const QByteArray &head, HttpRequest &out)
{
    const QList<QByteArray> lines = head.split('\n');
    if (lines.isEmpty())
        return false;

    const QByteArray requestLine = lines.first().trimmed();
    const QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 2)
        return false;

    out.method = QString::fromLatin1(parts.at(0)).toUpper();

    const QString target = QString::fromUtf8(parts.at(1));
    const int questionMark = target.indexOf(QLatin1Char('?'));
    if (questionMark >= 0) {
        out.path = QUrl::fromPercentEncoding(target.left(questionMark).toUtf8());
        const QString queryString = target.mid(questionMark + 1);
        const auto pairs = queryString.split(QLatin1Char('&'), Qt::SkipEmptyParts);
        for (const QString &pair : pairs) {
            const int equals = pair.indexOf(QLatin1Char('='));
            if (equals <= 0)
                continue;
            out.query.insert(
                QUrl::fromPercentEncoding(pair.left(equals).toUtf8()),
                QUrl::fromPercentEncoding(pair.mid(equals + 1).toUtf8()));
        }
    } else {
        out.path = QUrl::fromPercentEncoding(target.toUtf8());
    }

    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        if (line.isEmpty())
            continue;
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        out.headers.insert(
            QString::fromLatin1(line.left(colon)).trimmed().toLower(),
            QString::fromUtf8(line.mid(colon + 1)).trimmed());
    }
    return true;
}

bool parseByteRange(const QString &value, qint64 fileSize, qint64 &start, qint64 &end)
{
    if (!value.startsWith(QLatin1String("bytes="), Qt::CaseInsensitive))
        return false;

    const QString spec = value.mid(6).trimmed();
    const int dash = spec.indexOf(QLatin1Char('-'));
    if (dash < 0)
        return false;

    const QString startText = spec.left(dash).trimmed();
    const QString endText = spec.mid(dash + 1).trimmed();

    // Suffix form ("bytes=-500"): the last N bytes.
    if (startText.isEmpty()) {
        bool ok = false;
        const qint64 suffix = endText.toLongLong(&ok);
        if (!ok || suffix <= 0)
            return false;
        start = qMax<qint64>(0, fileSize - suffix);
        end = fileSize - 1;
        return start <= end;
    }

    bool ok = false;
    start = startText.toLongLong(&ok);
    if (!ok || start < 0 || start >= fileSize)
        return false;

    if (endText.isEmpty()) {
        end = fileSize - 1;
    } else {
        bool endOk = false;
        end = endText.toLongLong(&endOk);
        if (!endOk)
            return false;
        end = qMin(end, fileSize - 1);
    }
    return start <= end;
}

HttpServer::HttpServer(QObject *parent)
    : QTcpServer(parent)
{
}

bool HttpServer::start(quint16 port)
{
    if (isListening())
        return true;
    return listen(QHostAddress::Any, port);
}

void HttpServer::stop()
{
    if (isListening())
        close();
}

void HttpServer::incomingConnection(qintptr socketDescriptor)
{
    auto *socket = new QTcpSocket(this);
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        socket->deleteLater();
        return;
    }

    m_states.insert(socket, ConnectionState{});

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { handleReadyRead(socket); });
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QObject::destroyed, this, [this, socket]() { m_states.remove(socket); });
}

void HttpServer::handleReadyRead(QTcpSocket *socket)
{
    auto it = m_states.find(socket);
    if (it == m_states.end())
        return;
    ConnectionState *state = &it.value();
    state->buffer.append(socket->readAll());

    if (!state->headParsed) {
        const int headEnd = state->buffer.indexOf("\r\n\r\n");
        if (headEnd < 0) {
            if (state->buffer.size() > kMaxHeadBytes) {
                sendResponse(socket, state->request, HttpResponse::error(400, QStringLiteral("bad_request")));
            }
            return; // still waiting for the rest of the head
        }

        const QByteArray head = state->buffer.left(headEnd);
        if (!parseRequestHead(head, state->request)) {
            sendResponse(socket, state->request, HttpResponse::error(400, QStringLiteral("bad_request")));
            return;
        }

        state->buffer.remove(0, headEnd + 4);
        state->headParsed = true;

        bool ok = false;
        const qint64 declared = state->request.header(QStringLiteral("content-length")).toLongLong(&ok);
        state->expectedBody = ok ? declared : 0;

        if (state->expectedBody > kMaxBodyBytes) {
            sendResponse(socket, state->request, HttpResponse::error(413, QStringLiteral("too_large")));
            return;
        }
    }

    if (state->buffer.size() < state->expectedBody)
        return; // body still arriving

    state->request.body = state->buffer.left(static_cast<int>(state->expectedBody));
    dispatch(socket, state->request);
}

void HttpServer::dispatch(QTcpSocket *socket, const HttpRequest &request)
{
    if (!m_handler) {
        sendResponse(socket, request, HttpResponse::error(500, QStringLiteral("no_handler")));
        return;
    }
    sendResponse(socket, request, m_handler(request));
}

void HttpServer::sendResponse(QTcpSocket *socket, const HttpRequest &request,
                              const HttpResponse &response)
{
    if (!response.filePath.isEmpty()) {
        streamFile(socket, request, response);
        return;
    }

    QByteArray head;
    head += QStringLiteral("HTTP/1.1 %1 %2\r\n")
                .arg(response.status)
                .arg(QLatin1String(reasonPhrase(response.status)))
                .toLatin1();
    head += QStringLiteral("Content-Type: %1\r\n").arg(response.contentType).toLatin1();
    head += QStringLiteral("Content-Length: %1\r\n").arg(response.body.size()).toLatin1();
    for (auto it = response.extraHeaders.constBegin(); it != response.extraHeaders.constEnd(); ++it)
        head += QStringLiteral("%1: %2\r\n").arg(it.key(), it.value()).toLatin1();
    head += "Connection: close\r\n\r\n";

    socket->write(head);
    if (request.method != QLatin1String("HEAD"))
        socket->write(response.body);
    socket->flush();
    socket->disconnectFromHost();
}

void HttpServer::streamFile(QTcpSocket *socket, const HttpRequest &request,
                            const HttpResponse &response)
{
    auto *file = new QFile(response.filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        sendResponse(socket, request, HttpResponse::error(404, QStringLiteral("not_found")));
        return;
    }

    const qint64 fileSize = file->size();
    qint64 start = 0;
    qint64 end = fileSize - 1;
    int status = 200;

    const QString rangeHeader = request.header(QStringLiteral("range"));
    if (!rangeHeader.isEmpty()) {
        if (parseByteRange(rangeHeader, fileSize, start, end)) {
            status = 206;
        } else {
            file->close();
            delete file;
            HttpResponse r = HttpResponse::error(416, QStringLiteral("bad_range"));
            r.extraHeaders.insert(QStringLiteral("Content-Range"),
                                  QStringLiteral("bytes */%1").arg(fileSize));
            sendResponse(socket, request, r);
            return;
        }
    }

    const qint64 length = end - start + 1;
    file->seek(start);

    QByteArray head;
    head += QStringLiteral("HTTP/1.1 %1 %2\r\n")
                .arg(status)
                .arg(QLatin1String(reasonPhrase(status)))
                .toLatin1();
    head += QStringLiteral("Content-Type: %1\r\n").arg(response.contentType).toLatin1();
    head += QStringLiteral("Content-Length: %1\r\n").arg(length).toLatin1();
    head += "Accept-Ranges: bytes\r\n";
    if (status == 206) {
        head += QStringLiteral("Content-Range: bytes %1-%2/%3\r\n")
                    .arg(start).arg(end).arg(fileSize).toLatin1();
    }
    for (auto it = response.extraHeaders.constBegin(); it != response.extraHeaders.constEnd(); ++it)
        head += QStringLiteral("%1: %2\r\n").arg(it.key(), it.value()).toLatin1();
    head += "Connection: close\r\n\r\n";
    socket->write(head);

    if (request.method == QLatin1String("HEAD")) {
        file->close();
        delete file;
        socket->flush();
        socket->disconnectFromHost();
        return;
    }

    // Streamed in chunks driven by bytesWritten: a big track must never be
    // buffered whole, and the event loop has to stay responsive.
    auto *remaining = new qint64(length);
    file->setParent(socket);

    auto writeChunk = [socket, file, remaining]() {
        if (*remaining <= 0)
            return;
        const QByteArray chunk = file->read(qMin(kChunkBytes, *remaining));
        if (chunk.isEmpty()) {
            *remaining = 0;
            socket->disconnectFromHost();
            return;
        }
        *remaining -= chunk.size();
        socket->write(chunk);
        if (*remaining <= 0)
            socket->disconnectFromHost();
    };

    connect(socket, &QTcpSocket::bytesWritten, socket, [writeChunk](qint64) { writeChunk(); });
    connect(socket, &QObject::destroyed, [remaining]() { delete remaining; });
    writeChunk();
}

} // namespace lumen::sync
