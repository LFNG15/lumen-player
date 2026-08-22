#ifndef ADDMUSICPAGE_H
#define ADDMUSICPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProcess>
#include "theme.h"
#include "trackmodel.h"

struct PendingFile {
    QString id;
    QString title;
    QString artist;
    QString filePath;
    qint64 fileSize;
    Theme::GradientPair palette;
};

class AddMusicPage : public QWidget {
    Q_OBJECT
public:
    explicit AddMusicPage(TrackModel *model, QWidget *parent = nullptr);
    void refresh();

signals:
    void trackAdded(const Track &track);
    void navigateBack();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void processFiles(const QStringList &paths);
    void refreshFileList();
    void addAllToLibrary();
    void startDownload();
    void startPlaylistImport();
    void applyResponsiveLayout();

    // User-configurable folder where YouTube downloads are saved (persisted in
    // QSettings; defaults to the system Music folder under "Lumen Music").
    QString downloadDir() const;
    void chooseDownloadFolder();
    void updateDownloadFolderLabel();

    TrackModel *m_model;
    QList<PendingFile> m_pendingFiles;

    QWidget     *m_dropZone;
    QLabel      *m_dropLabel;
    QVBoxLayout *m_fileListLayout;
    QWidget     *m_fileListContainer;
    QComboBox   *m_folderCombo;
    QLineEdit   *m_newFolderEdit;
    QBoxLayout  *m_folderRow = nullptr;
    QPushButton *m_addBtn;
    QWidget     *m_folderSection;
    bool         m_isDragOver = false;

    QLineEdit   *m_importUrlEdit    = nullptr;
    QLabel      *m_importStatus     = nullptr;
    QLineEdit   *m_urlEdit          = nullptr;
    QPushButton *m_downloadBtn      = nullptr;
    QLabel      *m_downloadStatus   = nullptr;
    QLabel      *m_downloadFolderLabel = nullptr;
    QProcess    *m_downloadProcess  = nullptr;
    QString      m_downloadPrefix;
    QString      m_lastDownloadOutput;
    QString      m_downloadFolderFullText;
    bool         m_downloadRetried = false;
};

#endif // ADDMUSICPAGE_H