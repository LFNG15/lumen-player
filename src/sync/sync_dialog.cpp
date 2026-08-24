#include "sync_dialog.h"

#include "design/stylesheet.h"
#include "widgets/lang.h"
#include "widgets/theme.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QThread>
#include <QVBoxLayout>

namespace lumen::sync {

SyncDialog::SyncDialog(const QString &dbPath, QWidget *parent)
    : QDialog(parent)
    , m_dbPath(dbPath)
{
    setWindowTitle(Lang::tr(QStringLiteral("Sincronizar com o celular")));
    setMinimumWidth(460);
    buildUi();
    applyTheme();

    // The server gets its own thread: serialising the library and streaming a
    // whole track must never block the UI.
    m_thread = new QThread(this);
    m_server = new SyncServer;
    m_server->moveToThread(m_thread);

    connect(m_thread, &QThread::finished, m_server, &QObject::deleteLater);
    connect(m_server, &SyncServer::started, this, &SyncDialog::onStarted);
    connect(m_server, &SyncServer::startFailed, this, &SyncDialog::onStartFailed);
    connect(m_server, &SyncServer::stopped, this, &SyncDialog::onStopped);
    connect(m_server, &SyncServer::pinChanged, this, &SyncDialog::onPinChanged);
    connect(m_server, &SyncServer::devicePaired, this, &SyncDialog::onDevicePaired);
    connect(m_server, &SyncServer::deviceListChanged, this, &SyncDialog::onDeviceListChanged);
    connect(m_server, &SyncServer::libraryChangedExternally,
            this, &SyncDialog::libraryChangedExternally);

    m_thread->start();
}

SyncDialog::~SyncDialog()
{
    if (m_server)
        QMetaObject::invokeMethod(m_server, "stopServer", Qt::BlockingQueuedConnection);
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(3000);
    }
}

void SyncDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(12);

    auto *title = new QLabel(Lang::tr(QStringLiteral("Sincronizar com o celular")), this);
    title->setFont(lumen::design::ThemeManager::t().type.title);
    root->addWidget(title);

    m_statusLabel = new QLabel(
        Lang::tr(QStringLiteral("A sincronização está desligada.")), this);
    m_statusLabel->setWordWrap(true);
    root->addWidget(m_statusLabel);

    m_toggleButton = new QPushButton(Lang::tr(QStringLiteral("Ligar sincronização")), this);
    m_toggleButton->setObjectName(QStringLiteral("lumenAccentBtn"));
    m_toggleButton->setCursor(Qt::PointingHandCursor);
    connect(m_toggleButton, &QPushButton::clicked, this, &SyncDialog::onToggleServer);
    root->addWidget(m_toggleButton);

    m_addressLabel = new QLabel(this);
    m_addressLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_addressLabel->hide();
    root->addWidget(m_addressLabel);

    m_pinLabel = new QLabel(this);
    m_pinLabel->setAlignment(Qt::AlignCenter);
    m_pinLabel->hide();
    root->addWidget(m_pinLabel);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->hide();
    root->addWidget(m_hintLabel);

    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    root->addWidget(line);

    auto *devicesTitle = new QLabel(Lang::tr(QStringLiteral("Aparelhos pareados")), this);
    devicesTitle->setObjectName(QStringLiteral("lumenMicroLabel"));
    root->addWidget(devicesTitle);

    m_deviceContainer = new QWidget(this);
    m_deviceLayout = new QVBoxLayout(m_deviceContainer);
    m_deviceLayout->setContentsMargins(0, 0, 0, 0);
    m_deviceLayout->setSpacing(6);
    root->addWidget(m_deviceContainer);

    root->addStretch(1);

    auto *closeBtn = new QPushButton(Lang::tr(QStringLiteral("Fechar")), this);
    closeBtn->setObjectName(QStringLiteral("lumenGhostBtn"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(closeBtn, 0, Qt::AlignRight);
}

void SyncDialog::applyTheme()
{
    lumen::design::StyleSheet::apply(this, QString(
        "QDialog { background: %1; color: %2; }")
        .arg(Theme::bg().name(), Theme::text().name()));

    lumen::design::StyleSheet::apply(m_statusLabel, QString(
        "color: %1; background: transparent;").arg(Theme::textSoft().name()));

    lumen::design::StyleSheet::apply(m_pinLabel, QString(
        "color: %1; background: %2; border: 1px solid %3; border-radius: 10px; "
        "padding: 14px; font-size: 30px; font-weight: 800; letter-spacing: 6px;")
        .arg(Theme::accent().name(), Theme::card().name(), Theme::border().name()));

    lumen::design::StyleSheet::apply(m_hintLabel, QString(
        "color: %1; background: transparent;").arg(Theme::textMuted().name()));

    lumen::design::StyleSheet::apply(m_addressLabel, QString(
        "color: %1; background: transparent;").arg(Theme::textSoft().name()));
}

void SyncDialog::onToggleServer()
{
    if (!m_running) {
        QMetaObject::invokeMethod(m_server, "startServer", Qt::QueuedConnection,
                                  Q_ARG(QString, m_dbPath),
                                  Q_ARG(quint16, kDefaultHttpPort));
    } else {
        QMetaObject::invokeMethod(m_server, "stopServer", Qt::QueuedConnection);
    }
}

void SyncDialog::onStarted(const QString &serverIdValue, const QString &address, quint16 port)
{
    Q_UNUSED(serverIdValue);
    m_running = true;
    m_toggleButton->setText(Lang::tr(QStringLiteral("Desligar sincronização")));
    m_statusLabel->setText(Lang::tr(
        QStringLiteral("Sincronização ligada. Abra o Lumen Music no celular, "
                       "na mesma rede Wi-Fi, e escolha este computador.")));

    m_addressLabel->setText(QStringLiteral("%1:%2").arg(address).arg(port));
    m_addressLabel->show();

    // Arm a PIN right away: the user opened this dialog to pair something.
    QMetaObject::invokeMethod(m_server, "armPairing", Qt::QueuedConnection);

    m_hintLabel->setText(Lang::tr(
        QStringLiteral("Se o computador não aparecer na lista do celular, digite o "
                       "endereço acima manualmente. Na primeira vez o Firewall do "
                       "Windows pede permissão: autorize para redes privadas.")));
    m_hintLabel->show();
}

void SyncDialog::onStartFailed(const QString &reason)
{
    m_running = false;
    m_statusLabel->setText(
        Lang::tr(QStringLiteral("Não foi possível ligar a sincronização: ")) + reason);
}

void SyncDialog::onStopped()
{
    m_running = false;
    m_toggleButton->setText(Lang::tr(QStringLiteral("Ligar sincronização")));
    m_statusLabel->setText(Lang::tr(QStringLiteral("A sincronização está desligada.")));
    m_addressLabel->hide();
    m_pinLabel->hide();
    m_hintLabel->hide();
}

void SyncDialog::onPinChanged(const QString &pin)
{
    if (pin.isEmpty()) {
        m_pinLabel->hide();
        return;
    }
    // Grouped as 123 456 so it is easy to read out loud.
    m_pinLabel->setText(pin.left(3) + QStringLiteral(" ") + pin.mid(3));
    m_pinLabel->show();
}

void SyncDialog::onDevicePaired(const QString &deviceName)
{
    m_statusLabel->setText(
        Lang::tr(QStringLiteral("Aparelho pareado: ")) + deviceName);
    // A used PIN is burnt; arm a fresh one for the next device.
    QMetaObject::invokeMethod(m_server, "armPairing", Qt::QueuedConnection);
}

void SyncDialog::onDeviceListChanged(const QVector<PairedDevice> &devices)
{
    rebuildDeviceList(devices);
}

void SyncDialog::rebuildDeviceList(const QVector<PairedDevice> &devices)
{
    while (QLayoutItem *item = m_deviceLayout->takeAt(0)) {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    if (devices.isEmpty()) {
        auto *empty = new QLabel(Lang::tr(QStringLiteral("Nenhum aparelho pareado ainda")),
                                 m_deviceContainer);
        lumen::design::StyleSheet::apply(empty, QString(
            "color: %1; background: transparent;").arg(Theme::textMuted().name()));
        m_deviceLayout->addWidget(empty);
        return;
    }

    for (const PairedDevice &device : devices) {
        auto *row = new QWidget(m_deviceContainer);
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);

        const QString when = device.lastSyncAt > 0
            ? QDateTime::fromMSecsSinceEpoch(device.lastSyncAt).toString(QStringLiteral("dd/MM HH:mm"))
            : Lang::tr(QStringLiteral("nunca"));

        auto *label = new QLabel(
            QStringLiteral("%1 — %2").arg(device.name.isEmpty() ? device.deviceId : device.name, when),
            row);
        lumen::design::StyleSheet::apply(label, QString(
            "color: %1; background: transparent;").arg(Theme::text().name()));
        layout->addWidget(label, 1);

        auto *revoke = new QPushButton(Lang::tr(QStringLiteral("Remover")), row);
        revoke->setObjectName(QStringLiteral("lumenGhostBtn"));
        revoke->setCursor(Qt::PointingHandCursor);
        const QString deviceId = device.deviceId;
        connect(revoke, &QPushButton::clicked, this, [this, deviceId]() {
            QMetaObject::invokeMethod(m_server, "revokeDevice", Qt::QueuedConnection,
                                      Q_ARG(QString, deviceId));
        });
        layout->addWidget(revoke);

        m_deviceLayout->addWidget(row);
    }
}

} // namespace lumen::sync
