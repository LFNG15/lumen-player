#ifndef LUMEN_SYNC_SYNC_DIALOG_H
#define LUMEN_SYNC_SYNC_DIALOG_H

#include <QDialog>
#include <QVector>

#include "sync_server.h"

class QLabel;
class QPushButton;
class QThread;
class QVBoxLayout;

namespace lumen::sync {

// "Sync with your phone" dialog.
//
// Owns the server thread: the server only runs while the user wants it to. It
// starts off by default — a music player has no business listening on the
// network until asked.
class SyncDialog : public QDialog {
    Q_OBJECT
public:
    explicit SyncDialog(const QString &dbPath, QWidget *parent = nullptr);
    ~SyncDialog() override;

signals:
    // Forwarded from the server so the main window can reload its views after a
    // phone pushes likes or playlists.
    void libraryChangedExternally();

private slots:
    void onToggleServer();
    void onStarted(const QString &serverId, const QString &address, quint16 port);
    void onStartFailed(const QString &reason);
    void onStopped();
    void onPinChanged(const QString &pin);
    void onDevicePaired(const QString &deviceName);
    void onDeviceListChanged(const QVector<lumen::sync::PairedDevice> &devices);

private:
    void buildUi();
    void applyTheme();
    void rebuildDeviceList(const QVector<PairedDevice> &devices);

    QString      m_dbPath;
    QThread     *m_thread = nullptr;
    SyncServer  *m_server = nullptr;

    QLabel      *m_statusLabel = nullptr;
    QLabel      *m_addressLabel = nullptr;
    QLabel      *m_pinLabel = nullptr;
    QLabel      *m_hintLabel = nullptr;
    QPushButton *m_toggleButton = nullptr;
    QWidget     *m_deviceContainer = nullptr;
    QVBoxLayout *m_deviceLayout = nullptr;
    bool         m_running = false;
};

} // namespace lumen::sync

#endif // LUMEN_SYNC_SYNC_DIALOG_H
