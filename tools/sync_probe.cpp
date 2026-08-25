// Ferramenta de desenvolvimento: sobe o servidor de sync sem abrir a interface.
//
// Serve para testar o cliente Android de ponta a ponta (e para depurar o
// protocolo com curl) sem depender de automatizar a janela do app.
//
//   sync_probe <caminho-do-vinil.db> [porta]
//
// Imprime o PIN de pareamento na saída padrão e continua servindo até Ctrl+C.
// Não faz parte do app: é compilado junto com os testes.

#include <QCoreApplication>
#include <QDebug>
#include <QTextStream>

#include "sync/sync_server.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QStringLiteral("2.1.0-dev"));

    QTextStream out(stdout);

    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 2) {
        out << "uso: sync_probe <caminho-do-vinil.db> [porta]\n";
        out.flush();
        return 2;
    }

    const QString dbPath = args.at(1);
    const quint16 port = args.size() > 2
        ? static_cast<quint16>(args.at(2).toUShort())
        : lumen::sync::kDefaultHttpPort;

    auto *server = new lumen::sync::SyncServer(&app);

    QObject::connect(server, &lumen::sync::SyncServer::started,
                     [&out](const QString &serverId, const QString &address, quint16 boundPort) {
        out << "servidor no ar em " << address << ":" << boundPort << "\n";
        out << "serverId: " << serverId << "\n";
        out.flush();
    });

    QObject::connect(server, &lumen::sync::SyncServer::startFailed,
                     [&out](const QString &reason) {
        out << "FALHOU: " << reason << "\n";
        out.flush();
        QCoreApplication::exit(1);
    });

    QObject::connect(server, &lumen::sync::SyncServer::pinChanged,
                     [&out](const QString &pin) {
        if (pin.isEmpty())
            out << "PIN consumido\n";
        else
            out << "PIN: " << pin << "\n";
        out.flush();
    });

    QObject::connect(server, &lumen::sync::SyncServer::devicePaired,
                     [&out, server](const QString &deviceName) {
        out << "aparelho pareado: " << deviceName << "\n";
        out.flush();
        // Rearma para o próximo aparelho, como faz o diálogo.
        server->armPairing();
    });

    QObject::connect(server, &lumen::sync::SyncServer::libraryChangedExternally,
                     [&out]() {
        out << "biblioteca alterada por um push\n";
        out.flush();
    });

    server->startServer(dbPath, port);
    server->armPairing();

    return app.exec();
}
