#include "addmusicpage.h"
#include "lang.h"
#include <QLabel>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QScrollArea>
#include <QFrame>
#include <QUuid>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDateTime>
#include <QProcessEnvironment>
#include <QSettings>
#include <algorithm>
#include "importplaylistdialog.h"
#include "mediatools.h"

AddMusicPage::AddMusicPage(TrackModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    setAcceptDrops(true);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    auto *content = new QWidget();
    content->setStyleSheet("background: transparent;");
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(12);

    auto *backBtn = new QPushButton("←");
    backBtn->setFixedSize(34, 34);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(QString(
        "QPushButton { background: rgba(255,255,255,0.05); color: %1; border: none; border-radius: 17px; font-size: 16px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }"
    ).arg(Theme::text().name()));
    connect(backBtn, &QPushButton::clicked, this, &AddMusicPage::navigateBack);
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    auto *title = new QLabel(Lang::tr("Inserção de Músicas"));
    title->setFont(Theme::titleFont(28));
    title->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::text().name()));
    layout->addWidget(title);

    auto *subtitle = new QLabel(Lang::tr("Arraste seus arquivos de áudio ou clique para selecionar"));
    subtitle->setFont(Theme::bodyFont(13));
    subtitle->setStyleSheet(QString("color: %1; background: transparent; padding-bottom: 12px;").arg(Theme::textSoft().name()));
    layout->addWidget(subtitle);

    auto *urlCard = new QWidget();
    urlCard->setObjectName("urlCard");
    urlCard->setStyleSheet(QString("QWidget#urlCard { background: %1; border-radius: 12px; }")
        .arg(Theme::card().name()));

    auto *urlCardLayout = new QVBoxLayout(urlCard);
    urlCardLayout->setContentsMargins(16, 14, 16, 14);
    urlCardLayout->setSpacing(8);

    auto *urlHeader = new QLabel("\uE774  Download via YouTube");
    urlHeader->setFont(Theme::bodyFont(12));
    urlHeader->setStyleSheet(QString("color: %1; background: transparent; font-weight: bold;")
        .arg(Theme::accent().name()));
    urlCardLayout->addWidget(urlHeader);

    auto *urlRow = new QHBoxLayout();
    urlRow->setSpacing(8);

    m_urlEdit = new QLineEdit();
    m_urlEdit->setPlaceholderText("https://www.youtube.com/watch?v=...");
    m_urlEdit->setFont(Theme::bodyFont(12));
    m_urlEdit->setStyleSheet(QString(R"(
        QLineEdit {
            background: %1; color: %2; border: 1px solid %3;
            border-radius: 8px; padding: 8px 12px;
        }
        QLineEdit:focus { border-color: %4; }
    )").arg(Theme::surface().name(), Theme::text().name(),
            Theme::border().name(), Theme::accent().name()));
    urlRow->addWidget(m_urlEdit, 1);

    m_downloadBtn = new QPushButton(Lang::tr("Baixar"));
    m_downloadBtn->setFixedSize(80, 36);
    m_downloadBtn->setCursor(Qt::PointingHandCursor);
    m_downloadBtn->setFont(Theme::bodyFont(12));
    m_downloadBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: none; border-radius: 8px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:disabled { background: %4; color: %5; }"
    ).arg(Theme::accent().name(), Theme::bg().name(),
          Theme::accent().lighter(110).name(),
          Theme::border().name(), Theme::textMuted().name()));
    connect(m_downloadBtn, &QPushButton::clicked, this, &AddMusicPage::startDownload);
    connect(m_urlEdit, &QLineEdit::returnPressed,  this, &AddMusicPage::startDownload);
    urlRow->addWidget(m_downloadBtn);

    urlCardLayout->addLayout(urlRow);

    // Configurable destination folder for downloads.
    auto *downloadFolderRow = new QHBoxLayout();
    downloadFolderRow->setSpacing(8);

    m_downloadFolderLabel = new QLabel();
    m_downloadFolderLabel->setFont(Theme::bodyFont(11));
    m_downloadFolderLabel->setStyleSheet(QString("color: %1; background: transparent;")
        .arg(Theme::textMuted().name()));
    m_downloadFolderLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    downloadFolderRow->addWidget(m_downloadFolderLabel, 1);

    auto *changeFolderBtn = new QPushButton(Lang::tr("Alterar"));
    changeFolderBtn->setCursor(Qt::PointingHandCursor);
    changeFolderBtn->setFont(Theme::bodyFont(11));
    changeFolderBtn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 6px; padding: 3px 10px; }"
        "QPushButton:hover { color: %3; border-color: %3; }"
    ).arg(Theme::textSoft().name(), Theme::border().name(), Theme::accent().name()));
    connect(changeFolderBtn, &QPushButton::clicked, this, &AddMusicPage::chooseDownloadFolder);
    downloadFolderRow->addWidget(changeFolderBtn, 0);

    urlCardLayout->addLayout(downloadFolderRow);
    updateDownloadFolderLabel();

    m_downloadStatus = new QLabel();
    m_downloadStatus->setFont(Theme::bodyFont(11));
    m_downloadStatus->setStyleSheet(QString("color: %1; background: transparent;")
        .arg(Theme::textMuted().name()));
    m_downloadStatus->hide();
    urlCardLayout->addWidget(m_downloadStatus);

    layout->addWidget(urlCard);

    // ── Streaming playlist import card ──────────────────────
    auto *importCard = new QWidget();
    importCard->setObjectName("importCard");
    importCard->setStyleSheet(QString("QWidget#importCard { background: %1; border-radius: 12px; }")
        .arg(Theme::card().name()));

    auto *importCardLayout = new QVBoxLayout(importCard);
    importCardLayout->setContentsMargins(16, 14, 16, 14);
    importCardLayout->setSpacing(8);

    auto *importHeader = new QLabel(QString("  ") + Lang::tr("Converter playlist de streaming"));
    importHeader->setFont(Theme::bodyFont(12));
    importHeader->setStyleSheet(QString("color: %1; background: transparent; font-weight: bold; font-family: \"Segoe UI\", \"Segoe MDL2 Assets\";")
        .arg(Theme::accent().name()));
    importCardLayout->addWidget(importHeader);

    auto *importHint = new QLabel(Lang::tr("Cole o link de uma playlist do Spotify ou do YouTube para trazê-la para o Lumen Music."));
    importHint->setFont(Theme::bodyFont(11));
    importHint->setWordWrap(true);
    importHint->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    importCardLayout->addWidget(importHint);

    auto *importRow = new QHBoxLayout();
    importRow->setSpacing(8);

    m_importUrlEdit = new QLineEdit();
    m_importUrlEdit->setPlaceholderText("https://open.spotify.com/playlist/...");
    m_importUrlEdit->setFont(Theme::bodyFont(12));
    m_importUrlEdit->setStyleSheet(m_urlEdit->styleSheet());
    importRow->addWidget(m_importUrlEdit, 1);

    auto *importBtn = new QPushButton(Lang::tr("Importar"));
    importBtn->setFixedSize(80, 36);
    importBtn->setCursor(Qt::PointingHandCursor);
    importBtn->setFont(Theme::bodyFont(12));
    importBtn->setStyleSheet(m_downloadBtn->styleSheet());
    connect(importBtn, &QPushButton::clicked, this, &AddMusicPage::startPlaylistImport);
    connect(m_importUrlEdit, &QLineEdit::returnPressed, this, &AddMusicPage::startPlaylistImport);
    importRow->addWidget(importBtn);

    importCardLayout->addLayout(importRow);

    m_importStatus = new QLabel();
    m_importStatus->setFont(Theme::bodyFont(11));
    m_importStatus->setWordWrap(true);
    m_importStatus->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::danger().name()));
    m_importStatus->hide();
    importCardLayout->addWidget(m_importStatus);

    layout->addWidget(importCard);

    // Drop zone
    m_dropZone = new QWidget();
    m_dropZone->setFixedHeight(180);
    m_dropZone->setCursor(Qt::PointingHandCursor);
    m_dropZone->setStyleSheet(QString(
        "QWidget { border: 2px dashed %1; border-radius: 16px; background: rgba(255,255,255,0.01); }"
    ).arg(Theme::border().name()));

    auto *dropLayout = new QVBoxLayout(m_dropZone);
    dropLayout->setAlignment(Qt::AlignCenter);
    dropLayout->setSpacing(10);

    auto *uploadIcon = new QLabel("⬆");
    uploadIcon->setFont(QFont("Segoe UI", 32));
    uploadIcon->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    uploadIcon->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(uploadIcon);

    m_dropLabel = new QLabel(Lang::tr("Clique ou arraste arquivos de áudio"));
    m_dropLabel->setFont(Theme::bodyFont(14));
    m_dropLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::textSoft().name()));
    m_dropLabel->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(m_dropLabel);

    auto *formatLabel = new QLabel("Opus, WebM, M4A, MP3, OGG, FLAC, WAV");
    formatLabel->setFont(Theme::bodyFont(11));
    formatLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    formatLabel->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(formatLabel);

    // Make drop zone clickable
    m_dropZone->installEventFilter(this);
    // We'll handle click via mouse press
    layout->addWidget(m_dropZone);

    // Browse button
    auto *browseBtn = new QPushButton(Lang::tr("Procurar Arquivos"));
    browseBtn->setFixedSize(180, 40);
    browseBtn->setCursor(Qt::PointingHandCursor);
    browseBtn->setFont(Theme::bodyFont(13));
    browseBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 20px; font-weight: 600; }"
        "QPushButton:hover { background: %4; }"
    ).arg(Theme::card().name(), Theme::textSoft().name(), Theme::border().name(), Theme::cardHover().name()));
    connect(browseBtn, &QPushButton::clicked, [this]() {
        QStringList files = QFileDialog::getOpenFileNames(this, Lang::tr("Selecionar Músicas"), QString(),
            Lang::tr("Áudio (*.opus *.webm *.m4a *.mp3 *.ogg *.oga *.flac *.wav *.aac)"));
        if (!files.isEmpty()) processFiles(files);
    });
    layout->addWidget(browseBtn, 0, Qt::AlignLeft);
    layout->addSpacing(8);

    // File list container
    m_fileListContainer = new QWidget();
    m_fileListContainer->setStyleSheet("background: transparent;");
    m_fileListLayout = new QVBoxLayout(m_fileListContainer);
    m_fileListLayout->setContentsMargins(0, 0, 0, 0);
    m_fileListLayout->setSpacing(6);
    m_fileListContainer->hide();
    layout->addWidget(m_fileListContainer);

    // Folder section
    m_folderSection = new QWidget();
    m_folderSection->setStyleSheet("background: transparent;");
    auto *folderLayout = new QVBoxLayout(m_folderSection);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderLayout->setSpacing(8);

    auto *folderLabel = new QLabel(Lang::tr("PLAYLIST DE DESTINO"));
    folderLabel->setFont(Theme::bodyFont(11));
    folderLabel->setStyleSheet(QString("color: %1; background: transparent; font-weight: bold; letter-spacing: 1px;").arg(Theme::textSoft().name()));
    folderLayout->addWidget(folderLabel);

    auto *folderRow = new QHBoxLayout();
    folderRow->setSpacing(8);

    m_folderCombo = new QComboBox();
    m_folderCombo->setFont(Theme::bodyFont(13));
    m_folderCombo->setMinimumWidth(200);
    m_folderCombo->setStyleSheet(QString(R"(
        QComboBox {
            background: %1; color: %2; border: 1px solid %3;
            border-radius: 8px; padding: 10px 14px;
        }
        QComboBox::drop-down { border: none; width: 30px; }
        QComboBox::down-arrow { image: none; border: none; }
        QComboBox QAbstractItemView {
            background: %4; color: %2; border: 1px solid %3;
            selection-background-color: %5;
        }
    )").arg(Theme::surface().name(), Theme::text().name(), Theme::border().name(),
            Theme::card().name(), Theme::cardHover().name()));
    folderRow->addWidget(m_folderCombo);

    m_newFolderEdit = new QLineEdit();
    m_newFolderEdit->setPlaceholderText(Lang::tr("Nova playlist..."));
    m_newFolderEdit->setFont(Theme::bodyFont(13));
    m_newFolderEdit->setMinimumWidth(160);
    m_newFolderEdit->setStyleSheet(QString(R"(
        QLineEdit {
            background: %1; color: %2; border: 1px solid %3;
            border-radius: 8px; padding: 10px 14px;
        }
        QLineEdit:focus { border-color: %4; }
    )").arg(Theme::surface().name(), Theme::text().name(), Theme::border().name(), Theme::accent().name()));
    folderRow->addWidget(m_newFolderEdit);
    folderRow->addStretch();

    folderLayout->addLayout(folderRow);

    m_addBtn = new QPushButton(Lang::tr("Adicionar à Biblioteca"));
    m_addBtn->setFont(Theme::bodyFont(14));
    m_addBtn->setFixedHeight(46);
    m_addBtn->setMinimumWidth(220);
    m_addBtn->setCursor(Qt::PointingHandCursor);
    m_addBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: none; border-radius: 23px; font-weight: bold; padding: 0 32px; }"
        "QPushButton:hover { background: %3; }"
    ).arg(Theme::accent().name(), Theme::bg().name(), Theme::accent().lighter(110).name()));
    connect(m_addBtn, &QPushButton::clicked, this, &AddMusicPage::addAllToLibrary);
    folderLayout->addSpacing(4);
    folderLayout->addWidget(m_addBtn, 0, Qt::AlignLeft);

    m_folderSection->hide();
    layout->addWidget(m_folderSection);
    layout->addStretch();

    scroll->setWidget(content);
    outerLayout->addWidget(scroll);
}

void AddMusicPage::refresh() {
    // Remove downloaded files that were never committed to library
    QString downloadsDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/downloads";
    for (auto &pf : m_pendingFiles) {
        if (pf.filePath.startsWith(downloadsDir)) {
            QFile::remove(pf.filePath);
        }
    }
    m_pendingFiles.clear();
    refreshFileList();

    if (m_downloadStatus) {
        m_downloadStatus->hide();
        m_downloadStatus->clear();
    }
    if (m_urlEdit) m_urlEdit->clear();

    m_folderCombo->clear();
    m_folderCombo->addItem(Lang::tr("Sem playlist"));
    auto folders = m_model->folders();
    for (auto &f : folders) {
        m_folderCombo->addItem(f.name);
    }
}

void AddMusicPage::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        m_isDragOver = true;
        m_dropZone->setStyleSheet(QString(
            "QWidget { border: 2px dashed %1; border-radius: 16px; background: %2; }"
        ).arg(Theme::accent().name(), Theme::accentRgba(0.12)));
        m_dropLabel->setText(Lang::tr("Solte os arquivos aqui"));
    }
}

void AddMusicPage::dropEvent(QDropEvent *event) {
    m_isDragOver = false;
    m_dropZone->setStyleSheet(QString(
        "QWidget { border: 2px dashed %1; border-radius: 16px; background: rgba(255,255,255,0.01); }"
    ).arg(Theme::border().name()));
    m_dropLabel->setText(Lang::tr("Clique ou arraste arquivos de áudio"));

    QStringList paths;
    for (auto &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) paths.append(url.toLocalFile());
    }
    if (!paths.isEmpty()) processFiles(paths);
}

void AddMusicPage::dragLeaveEvent(QDragLeaveEvent *) {
    m_isDragOver = false;
    m_dropZone->setStyleSheet(QString(
        "QWidget { border: 2px dashed %1; border-radius: 16px; background: rgba(255,255,255,0.01); }"
    ).arg(Theme::border().name()));
    m_dropLabel->setText(Lang::tr("Clique ou arraste arquivos de áudio"));
}

void AddMusicPage::processFiles(const QStringList &paths) {
    const QStringList &audioExts = MediaTools::audioExts();

    for (auto &path : paths) {
        QFileInfo fi(path);
        if (!audioExts.contains(fi.suffix().toLower())) continue;

        PendingFile pf;
        pf.id = QUuid::createUuid().toString(QUuid::Id128).left(10);
        pf.filePath = path;
        pf.fileSize = fi.size();
        pf.palette = Theme::randomPalette();

        // Try to parse "Artist - Title" from filename
        QString baseName = fi.completeBaseName();
        QStringList parts = baseName.split(QRegularExpression("\\s*[-–—]\\s*"));
        if (parts.size() >= 2) {
            pf.artist = parts[0].trimmed();
            pf.title = parts.mid(1).join(" - ").trimmed();
        } else {
            pf.title = baseName;
            pf.artist = Lang::tr("Desconhecido");
        }

        m_pendingFiles.append(pf);
    }

    refreshFileList();
}

void AddMusicPage::refreshFileList() {
    // Clear list
    QLayoutItem *item;
    while ((item = m_fileListLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (m_pendingFiles.isEmpty()) {
        m_fileListContainer->hide();
        m_folderSection->hide();
        return;
    }

    m_fileListContainer->show();
    m_folderSection->show();

    auto *header = new QLabel(QString(Lang::tr("%1 arquivo%2 selecionado%2"))
        .arg(m_pendingFiles.size())
        .arg(m_pendingFiles.size() > 1 ? "s" : ""));
    header->setFont(Theme::bodyFont(14));
    header->setStyleSheet(QString("color: %1; background: transparent; font-weight: bold; padding-top: 8px;").arg(Theme::text().name()));
    m_fileListLayout->addWidget(header);

    QString inputStyle = QString(R"(
        QLineEdit {
            background: %1; color: %2; border: 1px solid %3;
            border-radius: 6px; padding: 5px 10px;
        }
        QLineEdit:focus { border-color: %4; }
    )").arg(Theme::surface().name(), Theme::text().name(),
            Theme::border().name(), Theme::accent().name());

    QString labelStyle = QString("color: %1; background: transparent; font-weight: bold; letter-spacing: 0.5px;")
        .arg(Theme::textMuted().name());

    for (int i = 0; i < m_pendingFiles.size(); ++i) {
        auto &pf = m_pendingFiles[i];

        auto *card = new QWidget();
        card->setObjectName("fileCard");
        card->setStyleSheet(QString("QWidget#fileCard { background: %1; border-radius: 10px; }").arg(Theme::card().name()));

        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(14);

        // Color swatch
        auto *swatch = new QWidget();
        swatch->setFixedSize(44, 44);
        swatch->setStyleSheet(QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 8px;")
            .arg(pf.palette.c1.name(), pf.palette.c2.name()));
        cardLayout->addWidget(swatch, 0, Qt::AlignTop);

        // Fields
        auto *fieldsLayout = new QVBoxLayout();
        fieldsLayout->setSpacing(6);
        fieldsLayout->setContentsMargins(0, 0, 0, 0);

        // Title field
        auto *titleRow = new QVBoxLayout();
        titleRow->setSpacing(2);
        auto *titleLabel = new QLabel(Lang::tr("Nome da música"));
        titleLabel->setFont(Theme::bodyFont(10));
        titleLabel->setStyleSheet(labelStyle);
        auto *titleEdit = new QLineEdit(pf.title);
        titleEdit->setFont(Theme::bodyFont(12));
        titleEdit->setStyleSheet(inputStyle);
        titleEdit->setPlaceholderText(Lang::tr("Título da música"));
        int idx = i;
        connect(titleEdit, &QLineEdit::textChanged, [this, idx](const QString &text) {
            if (idx < m_pendingFiles.size()) m_pendingFiles[idx].title = text;
        });
        titleRow->addWidget(titleLabel);
        titleRow->addWidget(titleEdit);
        fieldsLayout->addLayout(titleRow);

        // Artist field
        auto *artistRow = new QVBoxLayout();
        artistRow->setSpacing(2);
        auto *artistLabel = new QLabel(Lang::tr("Nome do artista"));
        artistLabel->setFont(Theme::bodyFont(10));
        artistLabel->setStyleSheet(labelStyle);
        auto *artistEdit = new QLineEdit(pf.artist);
        artistEdit->setFont(Theme::bodyFont(12));
        artistEdit->setStyleSheet(inputStyle);
        artistEdit->setPlaceholderText(Lang::tr("Nome do artista"));
        connect(artistEdit, &QLineEdit::textChanged, [this, idx](const QString &text) {
            if (idx < m_pendingFiles.size()) m_pendingFiles[idx].artist = text;
        });
        artistRow->addWidget(artistLabel);
        artistRow->addWidget(artistEdit);
        fieldsLayout->addLayout(artistRow);

        cardLayout->addLayout(fieldsLayout, 1);

        // Right side: size + remove
        auto *rightLayout = new QVBoxLayout();
        rightLayout->setSpacing(4);
        rightLayout->setContentsMargins(0, 0, 0, 0);

        auto *removeBtn = new QPushButton("✕");
        removeBtn->setFixedSize(28, 28);
        removeBtn->setCursor(Qt::PointingHandCursor);
        removeBtn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; border: none; font-size: 14px; border-radius: 14px; }"
            "QPushButton:hover { background: rgba(255,255,255,0.08); color: %2; }"
        ).arg(Theme::textMuted().name(), Theme::danger().name()));
        QString fileId = pf.id;
        connect(removeBtn, &QPushButton::clicked, [this, fileId]() {
            m_pendingFiles.erase(std::remove_if(m_pendingFiles.begin(), m_pendingFiles.end(),
                [&](const PendingFile &f) { return f.id == fileId; }), m_pendingFiles.end());
            refreshFileList();
        });
        rightLayout->addWidget(removeBtn, 0, Qt::AlignRight | Qt::AlignTop);

        auto *sizeLabel = new QLabel(QString("%1 MB").arg(pf.fileSize / (1024.0 * 1024.0), 0, 'f', 1));
        sizeLabel->setFont(Theme::monoFont(10));
        sizeLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
        sizeLabel->setAlignment(Qt::AlignRight);
        rightLayout->addWidget(sizeLabel, 0, Qt::AlignRight | Qt::AlignBottom);
        rightLayout->addStretch();

        cardLayout->addLayout(rightLayout);

        m_fileListLayout->addWidget(card);
    }

    m_addBtn->setText(QString(Lang::tr("Adicionar %1 Música%2"))
        .arg(m_pendingFiles.size())
        .arg(m_pendingFiles.size() > 1 ? "s" : ""));
}

QString AddMusicPage::downloadDir() const {
    return MediaTools::downloadDir();
}

void AddMusicPage::chooseDownloadFolder() {
    QString dir = QFileDialog::getExistingDirectory(
        this, Lang::tr("Escolher pasta de downloads"), downloadDir());
    if (dir.isEmpty()) return;            // user cancelled
    QSettings().setValue("downloadDir", dir);
    updateDownloadFolderLabel();
}

void AddMusicPage::updateDownloadFolderLabel() {
    if (m_downloadFolderLabel)
        m_downloadFolderLabel->setText(
            Lang::tr("Salvar em: ") + QDir::toNativeSeparators(downloadDir()));
}

void AddMusicPage::startDownload() {
    if (m_downloadProcess) return;

    QString url = m_urlEdit->text().trimmed();
    if (url.isEmpty()) return;

    if (!url.contains("youtube.com") && !url.contains("youtu.be")) {
        m_downloadStatus->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::danger().name()));
        m_downloadStatus->setText(Lang::tr("Use um link do YouTube (youtube.com ou youtu.be)."));
        m_downloadStatus->show();
        return;
    }

    QString outDir = downloadDir();

    // Unique prefix per download session — avoids any before/after comparison issues
    m_downloadPrefix = QString::number(QDateTime::currentMSecsSinceEpoch());

    m_downloadBtn->setEnabled(false);
    m_lastDownloadOutput.clear();
    m_downloadStatus->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    m_downloadStatus->setText(Lang::tr("Conectando..."));
    m_downloadStatus->show();

    m_downloadProcess = new QProcess(this);
    m_downloadProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_downloadProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QString out = QString::fromUtf8(m_downloadProcess->readAllStandardOutput()).trimmed();
        // buffer last meaningful line for error reporting
        for (const auto &line : out.split('\n')) {
            QString l = line.trimmed();
            if (!l.isEmpty()) m_lastDownloadOutput = l;
        }
        if (out.contains("[download]")) {
            QRegularExpression re(R"((\d+\.?\d*)%)");
            auto match = re.match(out);
            m_downloadStatus->setText(match.hasMatch()
                ? QString(Lang::tr("Baixando... %1%")).arg(match.captured(1))
                : Lang::tr("Baixando..."));
        } else if (out.contains("ExtractAudio") || out.contains("ffmpeg")) {
            m_downloadStatus->setText(Lang::tr("Convertendo para Opus..."));
        }
    });

    connect(m_downloadProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            m_downloadProcess = nullptr;
            m_downloadBtn->setEnabled(true);
            m_downloadStatus->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::danger().name()));
            m_downloadStatus->setText(Lang::tr("yt-dlp não encontrado. Instale com: pip install yt-dlp"));
        }
    });

    connect(m_downloadProcess, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        m_downloadProcess = nullptr;
        m_downloadBtn->setEnabled(true);

        if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
            m_downloadStatus->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::danger().name()));
            QString detail = m_lastDownloadOutput.isEmpty()
                ? Lang::tr("Verifique o link ou tente novamente.")
                : m_lastDownloadOutput;
            m_downloadStatus->setText(Lang::tr("Erro: ") + detail);
            m_downloadStatus->setWordWrap(true);
            return;
        }

        QString outDir = downloadDir();
        // Find files that match the unique prefix for this download session.
        // The extension depends on whether ffmpeg remuxed to .opus or we kept
        // the native stream (.webm/.m4a), so accept any known audio extension.
        QStringList added;
        for (const auto &f : QDir(outDir).entryList({m_downloadPrefix + "_*"}, QDir::Files)) {
            if (MediaTools::audioExts().contains(QFileInfo(f).suffix().toLower()))
                added.append(outDir + "/" + f);
        }

        if (added.isEmpty()) {
            m_downloadStatus->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::danger().name()));
            m_downloadStatus->setText(Lang::tr("Download concluído, mas nenhum arquivo de áudio encontrado."));
            return;
        }

        m_downloadStatus->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::accent().name()));
        m_downloadStatus->setText(QString(Lang::tr("Concluído! %1 arquivo%2 pronto%2 para adicionar."))
            .arg(added.size())
            .arg(added.size() > 1 ? "s" : ""));
        m_urlEdit->clear();
        processFiles(added);
    });


    const QString ytDlp = MediaTools::findYtDlp();
    if (ytDlp.isEmpty()) {
        m_downloadProcess->deleteLater();
        m_downloadProcess = nullptr;
        m_downloadBtn->setEnabled(true);
        m_downloadStatus->setStyleSheet(QString("color: %1; background: transparent;").arg(Theme::danger().name()));
        m_downloadStatus->setText(Lang::tr("yt-dlp não encontrado. Verifique sua pasta de instalação."));
        m_downloadStatus->show();
        return;
    }

    // ffmpeg is optional. The YouTube audio is already in the Opus codec, so no
    // re-encoding is ever needed — but turning it into a clean ".opus" file means
    // remuxing the WebM container, which only ffmpeg can do.
    //   - ffmpeg present: extract + remux into a tidy .opus (lossless copy).
    //   - ffmpeg absent : download the native audio stream as-is (Opus in .webm
    //                      when offered, otherwise bestaudio). It plays fine via
    //                      Qt's ffmpeg multimedia backend.
    const QString outTemplate = outDir + "/" + m_downloadPrefix + "_%(title)s.%(ext)s";
    m_downloadProcess->start(ytDlp, MediaTools::downloadArgs(url, outTemplate));
}

void AddMusicPage::startPlaylistImport() {
    QString url = m_importUrlEdit->text().trimmed();
    if (url.isEmpty()) return;

    if (!ImportPlaylistDialog::canImport(url)) {
        m_importStatus->setText(Lang::tr("Use o link de uma playlist do Spotify (open.spotify.com/playlist/...) ou do YouTube (com \"list=\")."));
        m_importStatus->show();
        return;
    }

    m_importStatus->hide();
    auto *dlg = new ImportPlaylistDialog(m_model, url, this);
    connect(dlg, &QDialog::finished, this, [this](int) {
        m_importUrlEdit->clear();
        // Repopulate the destination-playlist combo so a freshly imported
        // playlist shows up without leaving the page.
        m_folderCombo->clear();
        m_folderCombo->addItem(Lang::tr("Sem playlist"));
        for (auto &f : m_model->folders()) m_folderCombo->addItem(f.name);
    });
    dlg->exec();
}

void AddMusicPage::addAllToLibrary() {
    if (m_pendingFiles.isEmpty()) return;

    QString folder = m_newFolderEdit->text().trimmed();
    if (folder.isEmpty()) {
        folder = m_folderCombo->currentText();
        if (folder == Lang::tr("Sem playlist")) folder = "";
    }

    // Files freshly downloaded into the downloads root move into the chosen
    // playlist's subfolder, so each playlist's music stays together on disk.
    // Files the user picked from elsewhere are never moved.
    const QString downloadsRoot = QDir(MediaTools::downloadDir()).absolutePath();
    const QString playlistFolder = folder.isEmpty() ? QString() : MediaTools::playlistDir(folder);

    for (auto &pf : m_pendingFiles) {
        QString path = pf.filePath;
        if (!playlistFolder.isEmpty()
                && QFileInfo(path).absolutePath() == downloadsRoot) {
            QString dest = playlistFolder + "/" + QFileInfo(path).fileName();
            if (QFile::rename(path, dest)) path = dest;
        }

        Track t = Track::create(pf.title, pf.artist, folder, QUrl::fromLocalFile(path));
        t.cover = pf.palette;
        m_model->addTrack(t);
        emit trackAdded(t);
    }

    m_pendingFiles.clear();
    m_newFolderEdit->clear();
    refreshFileList();
    emit navigateBack();
}