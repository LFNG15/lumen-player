#include "design/stylesheet.h"
#include "folderspage.h"
#include "lang.h"
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QScrollArea>
#include <QFrame>
#include <QDialog>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QMenu>

FoldersPage::FoldersPage(TrackModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    lumen::design::StyleSheet::apply(scroll, "QScrollArea { background: transparent; border: none; }");

    auto *content = new QWidget();
    lumen::design::StyleSheet::apply(content, "background: transparent;");
    m_contentLayout = new QVBoxLayout(content);
    m_contentLayout->setContentsMargins(32, 28, 32, 28);
    m_contentLayout->setSpacing(12);

    scroll->setWidget(content);
    outerLayout->addWidget(scroll);
}

void FoldersPage::refresh() {
    QLayoutItem *item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    // Header row: title + create button
    auto *headerRow = new QHBoxLayout();
    auto *title = new QLabel(Lang::tr("Playlists"));
    title->setFont(Theme::titleFont(28));
    lumen::design::StyleSheet::apply(title, QString("color: %1; background: transparent;").arg(Theme::text().name()));
    headerRow->addWidget(title);
    headerRow->addStretch();

    auto *createBtn = new QPushButton(QString("\uE109  ") + Lang::tr(Lang::tr("Nova Playlist")));
    createBtn->setFixedHeight(36);
    createBtn->setCursor(Qt::PointingHandCursor);
    createBtn->setFont(Theme::bodyFont(12));
    lumen::design::StyleSheet::apply(createBtn, QString(
        "QPushButton { background: %1; color: %2; border: none; border-radius: 18px; padding: 0 16px; font-weight: bold; font-family: \"Segoe UI\", \"Segoe MDL2 Assets\"; }"
        "QPushButton:hover { background: %3; }"
    ).arg(Theme::accent().name(), Theme::bg().name(), Theme::accent().lighter(110).name()));
    connect(createBtn, &QPushButton::clicked, this, &FoldersPage::showCreateDialog);
    headerRow->addWidget(createBtn);

    auto *headerWidget = new QWidget();
    headerWidget->setLayout(headerRow);
    lumen::design::StyleSheet::apply(headerWidget, "background: transparent;");
    m_contentLayout->addWidget(headerWidget);
    m_contentLayout->addSpacing(8);

    auto folders = m_model->folders();
    auto standalone = m_model->standaloneTracks();

    if (folders.isEmpty() && standalone.isEmpty()) {
        auto *empty = new QLabel(Lang::tr("Nenhuma playlist ainda\nCrie uma playlist ou adicione músicas"));
        empty->setFont(Theme::bodyFont(14));
        lumen::design::StyleSheet::apply(empty, QString("color: %1; background: transparent; padding-top: 60px;").arg(Theme::textMuted().name()));
        empty->setAlignment(Qt::AlignCenter);
        m_contentLayout->addWidget(empty);
        m_contentLayout->addStretch();
        return;
    }

    auto *grid = new QGridLayout();
    grid->setSpacing(16);
    // Columns adapt to available content width (~ card 180 + gap 16).
    const int viewportW = qMax(200, width() - 64);
    const int cols = qBound(1, (viewportW + 16) / (180 + 16), 6);
    int col = 0, row = 0;

    // Standalone tracks card
    if (!standalone.isEmpty()) {
        auto *card = new QWidget();
        card->setFixedSize(180, 210);
        lumen::design::StyleSheet::apply(card, QString("background: %1; border-radius: 12px;").arg(Theme::card().name()));
        card->setCursor(Qt::PointingHandCursor);

        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 14, 14, 14);
        cardLayout->setSpacing(8);

        // Cover: mosaic or single
        auto *coverWidget = new QWidget();
        coverWidget->setFixedSize(152, 120);
        if (standalone.size() >= 4) {
            auto *coverGrid = new QGridLayout(coverWidget);
            coverGrid->setSpacing(2);
            coverGrid->setContentsMargins(0, 0, 0, 0);
            for (int i = 0; i < 4; ++i) {
                auto *cell = new QWidget();
                lumen::design::StyleSheet::apply(cell, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 4px;")
                    .arg(standalone[i].cover.c1.name(), standalone[i].cover.c2.name()));
                coverGrid->addWidget(cell, i / 2, i % 2);
            }
        } else {
            auto g = standalone[0].cover;
            lumen::design::StyleSheet::apply(coverWidget, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 8px;")
                .arg(g.c1.name(), g.c2.name()));
        }
        cardLayout->addWidget(coverWidget);

        auto *nameLabel = new QLabel(Lang::tr("Músicas avulsas"));
        nameLabel->setFont(Theme::bodyFont(14));
        lumen::design::StyleSheet::apply(nameLabel, QString("color: %1; background: transparent; font-weight: bold;").arg(Theme::text().name()));
        cardLayout->addWidget(nameLabel);

        auto *countLabel = new QLabel(QString(Lang::tr("%1 faixa%2")).arg(standalone.size()).arg(standalone.size() != 1 ? "s" : ""));
        countLabel->setFont(Theme::bodyFont(11));
        lumen::design::StyleSheet::apply(countLabel, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
        cardLayout->addWidget(countLabel);

        // Clickable overlay on top so clicks anywhere on the card navigate.
        auto *overlay = new QPushButton(card);
        overlay->setGeometry(0, 0, 180, 210);
        lumen::design::StyleSheet::apply(overlay, "background: transparent; border: none;");
        overlay->setCursor(Qt::PointingHandCursor);
        overlay->raise();
        connect(overlay, &QPushButton::clicked, [this]() { emit folderSelected(""); });

        grid->addWidget(card, row, col);
        col++;
        if (col >= cols) { col = 0; row++; }
    }

    for (auto &f : folders) {
        auto ft = m_model->tracksInFolder(f.name);
        int fid = f.id;
        QString fname = f.name;
        Theme::GradientPair fcover = f.cover;
        QString fcoverImage = f.coverImage;

        auto *card = new QWidget();
        card->setFixedSize(180, 210);
        lumen::design::StyleSheet::apply(card, QString("background: %1; border-radius: 12px;").arg(Theme::card().name()));
        card->setCursor(Qt::PointingHandCursor);

        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 14, 14, 14);
        cardLayout->setSpacing(8);

        // Cover: image (if set) > mosaic (4+ tracks) > folder gradient
        QPixmap coverPix = fcoverImage.isEmpty() ? QPixmap()
                                                 : Theme::roundedCover(fcoverImage, 152, 110, 8);
        auto *coverWidget = new QWidget();
        coverWidget->setFixedSize(152, 110);
        if (!coverPix.isNull()) {
            auto *imgLabel = new QLabel(coverWidget);
            imgLabel->setGeometry(0, 0, 152, 110);
            imgLabel->setPixmap(coverPix);
            lumen::design::StyleSheet::apply(imgLabel, "background: transparent;");
        } else if (ft.size() >= 4) {
            auto *coverGrid = new QGridLayout(coverWidget);
            coverGrid->setSpacing(2);
            coverGrid->setContentsMargins(0, 0, 0, 0);
            for (int i = 0; i < 4; ++i) {
                auto *cell = new QWidget();
                lumen::design::StyleSheet::apply(cell, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 4px;")
                    .arg(ft[i].cover.c1.name(), ft[i].cover.c2.name()));
                coverGrid->addWidget(cell, i / 2, i % 2);
            }
        } else {
            // Use folder's own cover colors
            lumen::design::StyleSheet::apply(coverWidget, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 8px;")
                .arg(fcover.c1.name(), fcover.c2.name()));
        }
        cardLayout->addWidget(coverWidget);

        // Name row with 3-dot menu button
        auto *nameRow = new QHBoxLayout();
        nameRow->setContentsMargins(0, 0, 0, 0);
        nameRow->setSpacing(4);

        auto *nameLabel = new QLabel(fname);
        nameLabel->setFont(Theme::bodyFont(13));
        lumen::design::StyleSheet::apply(nameLabel, QString("color: %1; background: transparent; font-weight: bold;").arg(Theme::text().name()));
        nameLabel->setWordWrap(false);
        nameRow->addWidget(nameLabel, 1);

        auto *menuBtn = new QPushButton("\uE712");
        menuBtn->setFixedSize(22, 22);
        menuBtn->setCursor(Qt::PointingHandCursor);
        menuBtn->setFont(Theme::iconFont(10));
        lumen::design::StyleSheet::apply(menuBtn, QString(
            "QPushButton { background: transparent; color: %1; border: none; border-radius: 4px; font-family: \"Segoe MDL2 Assets\"; }"
            "QPushButton:hover { background: rgba(255,255,255,0.1); color: %2; }"
        ).arg(Theme::textMuted().name(), Theme::text().name()));
        nameRow->addWidget(menuBtn);

        cardLayout->addLayout(nameRow);

        auto *countLabel = new QLabel(QString(Lang::tr("%1 faixa%2")).arg(ft.size()).arg(ft.size() != 1 ? "s" : ""));
        countLabel->setFont(Theme::bodyFont(11));
        lumen::design::StyleSheet::apply(countLabel, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
        cardLayout->addWidget(countLabel);

        // 3-dot menu
        connect(menuBtn, &QPushButton::clicked, [this, fid, fname, fcover, fcoverImage](bool) {
            auto *menu = new QMenu(this);
            lumen::design::StyleSheet::apply(menu, QString(
                "QMenu { background: %1; border: 1px solid %2; border-radius: 8px; padding: 4px; color: %3; }"
                "QMenu::item { padding: 8px 16px; border-radius: 4px; }"
                "QMenu::item:selected { background: %4; }"
            ).arg(Theme::card().name(), Theme::border().name(), Theme::text().name(), Theme::cardHover().name()));

            menu->addAction(QString("\uE70F  ") + Lang::tr("Renomear"), [this, fid, fname]() {
                showRenameDialog(fid, fname);
            });
            menu->addAction(QString("\uE771  ") + Lang::tr("Editar capa"), [this, fid, fcover, fcoverImage]() {
                showCoverDialog(fid, fcover, fcoverImage);
            });
            menu->addSeparator();
            menu->addAction(QString("\uE107  ") + Lang::tr("Excluir playlist"), [this, fid, fname]() {
                showDeleteConfirm(fid, fname);
            });
            menu->exec(QCursor::pos());
            menu->deleteLater();
        });

        // Clickable overlay on top so clicks anywhere on the card navigate.
        auto *overlay = new QPushButton(card);
        overlay->setGeometry(0, 0, 180, 210);
        lumen::design::StyleSheet::apply(overlay, "background: transparent; border: none;");
        overlay->setCursor(Qt::PointingHandCursor);
        overlay->raise();
        connect(overlay, &QPushButton::clicked, [this, fname]() { emit folderSelected(fname); });

        // Keep the 3-dot menu clickable above the overlay.
        menuBtn->raise();

        grid->addWidget(card, row, col);
        col++;
        if (col >= cols) { col = 0; row++; }
    }

    auto *gridWidget = new QWidget();
    gridWidget->setLayout(grid);
    lumen::design::StyleSheet::apply(gridWidget, "background: transparent;");
    m_contentLayout->addWidget(gridWidget);
    m_contentLayout->addStretch();
}

void FoldersPage::showCreateDialog() {
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(Lang::tr("Nova Playlist"));
    dlg->setFixedSize(380, 270);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    lumen::design::StyleSheet::apply(dlg, QString(
        "QDialog { background: %1; }"
        "QLabel { background: transparent; color: %2; }"
        "QLineEdit { background: %3; color: %2; border: 1px solid %4; border-radius: 8px; padding: 8px 12px; }"
        "QLineEdit:focus { border-color: %5; }"
    ).arg(Theme::surface().name(), Theme::text().name(), Theme::bg().name(),
          Theme::border().name(), Theme::accent().name()));

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(Lang::tr("Nome da playlist"));
    titleLabel->setFont(Theme::bodyFont(12));
    layout->addWidget(titleLabel);

    auto *nameEdit = new QLineEdit();
    nameEdit->setPlaceholderText(Lang::tr("Ex: Minha Playlist"));
    nameEdit->setFont(Theme::bodyFont(13));
    layout->addWidget(nameEdit);

    // Color pickers
    auto pal = Theme::randomPalette();
    auto *colorRow = new QHBoxLayout();
    auto *colorLabel = new QLabel(Lang::tr("Cores da capa:"));
    colorLabel->setFont(Theme::bodyFont(12));
    colorRow->addWidget(colorLabel);

    QColor *c1 = new QColor(pal.c1);
    QColor *c2 = new QColor(pal.c2);

    auto *btn1 = new QPushButton();
    btn1->setFixedSize(32, 32);
    btn1->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(btn1, QString("background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px;").arg(c1->name()));
    connect(btn1, &QPushButton::clicked, [btn1, c1, dlg]() {
        QColor chosen = QColorDialog::getColor(*c1, dlg, Lang::tr("Cor 1"));
        if (chosen.isValid()) {
            *c1 = chosen;
            lumen::design::StyleSheet::apply(btn1, QString("background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px;").arg(c1->name()));
        }
    });
    colorRow->addWidget(btn1);

    auto *btn2 = new QPushButton();
    btn2->setFixedSize(32, 32);
    btn2->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(btn2, QString("background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px;").arg(c2->name()));
    connect(btn2, &QPushButton::clicked, [btn2, c2, dlg]() {
        QColor chosen = QColorDialog::getColor(*c2, dlg, Lang::tr("Cor 2"));
        if (chosen.isValid()) {
            *c2 = chosen;
            lumen::design::StyleSheet::apply(btn2, QString("background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px;").arg(c2->name()));
        }
    });
    colorRow->addWidget(btn2);
    colorRow->addStretch();
    layout->addLayout(colorRow);

    // Optional cover image
    QString *imagePath = new QString();
    auto *imageRow = new QHBoxLayout();
    imageRow->setSpacing(8);

    auto *imgPreview = new QLabel();
    imgPreview->setFixedSize(50, 32);
    lumen::design::StyleSheet::apply(imgPreview, QString("background: %1; border-radius: 6px;").arg(Theme::bg().name()));
    imageRow->addWidget(imgPreview);

    auto *pickImageBtn = new QPushButton(Lang::tr("Escolher imagem"));
    pickImageBtn->setFont(Theme::bodyFont(11));
    pickImageBtn->setFixedHeight(32);
    pickImageBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(pickImageBtn, QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 8px; padding: 0 12px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.05); }"
    ).arg(Theme::textSoft().name(), Theme::border().name()));
    connect(pickImageBtn, &QPushButton::clicked, [dlg, imagePath, imgPreview]() {
        QString file = QFileDialog::getOpenFileName(dlg, Lang::tr("Escolher imagem da capa"), QString(),
            Lang::tr("Imagens (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (file.isEmpty()) return;
        *imagePath = file;
        QPixmap pm = Theme::roundedCover(file, 50, 32, 6);
        if (!pm.isNull()) imgPreview->setPixmap(pm);
    });
    imageRow->addWidget(pickImageBtn);
    imageRow->addStretch();
    layout->addLayout(imageRow);

    // Buttons
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(Lang::tr("Cancelar"));
    cancelBtn->setFont(Theme::bodyFont(12));
    cancelBtn->setFixedHeight(36);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(cancelBtn, QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 18px; padding: 0 16px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.05); }"
    ).arg(Theme::textSoft().name(), Theme::border().name()));
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    btnRow->addWidget(cancelBtn);

    auto *createBtn2 = new QPushButton(Lang::tr("Criar"));
    createBtn2->setFont(Theme::bodyFont(12));
    createBtn2->setFixedHeight(36);
    createBtn2->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(createBtn2, QString(
        "QPushButton { background: %1; color: %2; border: none; border-radius: 18px; padding: 0 20px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }"
    ).arg(Theme::accent().name(), Theme::bg().name(), Theme::accent().lighter(110).name()));
    connect(createBtn2, &QPushButton::clicked, [this, dlg, nameEdit, c1, c2, imagePath]() {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) return;
        m_model->createPlaylist(name, *c1, *c2, *imagePath);
        delete c1; delete c2; delete imagePath;
        dlg->accept();
    });
    btnRow->addWidget(createBtn2);
    layout->addLayout(btnRow);

    connect(dlg, &QDialog::rejected, [c1, c2, imagePath]() { delete c1; delete c2; delete imagePath; });
    connect(nameEdit, &QLineEdit::returnPressed, createBtn2, &QPushButton::click);
    dlg->exec();
}

void FoldersPage::showRenameDialog(int id, const QString &currentName) {
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(Lang::tr("Renomear Playlist"));
    dlg->setFixedSize(340, 140);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    lumen::design::StyleSheet::apply(dlg, QString(
        "QDialog { background: %1; }"
        "QLabel { background: transparent; color: %2; }"
        "QLineEdit { background: %3; color: %2; border: 1px solid %4; border-radius: 8px; padding: 8px 12px; }"
        "QLineEdit:focus { border-color: %5; }"
    ).arg(Theme::surface().name(), Theme::text().name(), Theme::bg().name(),
          Theme::border().name(), Theme::accent().name()));

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *nameEdit = new QLineEdit(currentName);
    nameEdit->setFont(Theme::bodyFont(13));
    nameEdit->selectAll();
    layout->addWidget(nameEdit);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(Lang::tr("Cancelar"));
    cancelBtn->setFont(Theme::bodyFont(12));
    cancelBtn->setFixedHeight(36);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(cancelBtn, QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 18px; padding: 0 16px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.05); }"
    ).arg(Theme::textSoft().name(), Theme::border().name()));
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    btnRow->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton(Lang::tr("Salvar"));
    saveBtn->setFont(Theme::bodyFont(12));
    saveBtn->setFixedHeight(36);
    saveBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(saveBtn, QString(
        "QPushButton { background: %1; color: %2; border: none; border-radius: 18px; padding: 0 20px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }"
    ).arg(Theme::accent().name(), Theme::bg().name(), Theme::accent().lighter(110).name()));
    connect(saveBtn, &QPushButton::clicked, [this, dlg, nameEdit, id]() {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) return;
        m_model->renamePlaylist(id, name);
        dlg->accept();
    });
    btnRow->addWidget(saveBtn);
    layout->addLayout(btnRow);

    connect(nameEdit, &QLineEdit::returnPressed, saveBtn, &QPushButton::click);
    dlg->exec();
}

void FoldersPage::showCoverDialog(int id, const Theme::GradientPair &current, const QString &currentImage) {
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(Lang::tr("Editar Capa"));
    dlg->setFixedSize(320, 230);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    lumen::design::StyleSheet::apply(dlg, QString("QDialog { background: %1; } QLabel { background: transparent; color: %2; }")
        .arg(Theme::surface().name(), Theme::text().name()));

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    auto *label = new QLabel(Lang::tr("Escolha as cores do gradiente:"));
    label->setFont(Theme::bodyFont(12));
    layout->addWidget(label);

    QColor *c1 = new QColor(current.c1);
    QColor *c2 = new QColor(current.c2);

    auto *colorRow = new QHBoxLayout();
    colorRow->setSpacing(12);

    auto *preview = new QWidget();
    preview->setFixedSize(60, 40);
    lumen::design::StyleSheet::apply(preview, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 8px;")
        .arg(c1->name(), c2->name()));
    colorRow->addWidget(preview);

    auto updatePreview = [preview, c1, c2]() {
        lumen::design::StyleSheet::apply(preview, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 8px;")
            .arg(c1->name(), c2->name()));
    };

    auto *btn1 = new QPushButton(Lang::tr("Cor 1"));
    btn1->setFixedSize(64, 36);
    btn1->setCursor(Qt::PointingHandCursor);
    btn1->setFont(Theme::bodyFont(11));
    lumen::design::StyleSheet::apply(btn1, QString("background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px; color: white;").arg(c1->name()));
    connect(btn1, &QPushButton::clicked, [btn1, c1, updatePreview, dlg]() {
        QColor chosen = QColorDialog::getColor(*c1, dlg, Lang::tr("Cor 1"));
        if (chosen.isValid()) {
            *c1 = chosen;
            lumen::design::StyleSheet::apply(btn1, QString("background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px; color: white;").arg(c1->name()));
            updatePreview();
        }
    });
    colorRow->addWidget(btn1);

    auto *btn2 = new QPushButton(Lang::tr("Cor 2"));
    btn2->setFixedSize(64, 36);
    btn2->setCursor(Qt::PointingHandCursor);
    btn2->setFont(Theme::bodyFont(11));
    lumen::design::StyleSheet::apply(btn2, QString("background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px; color: white;").arg(c2->name()));
    connect(btn2, &QPushButton::clicked, [btn2, c2, updatePreview, dlg]() {
        QColor chosen = QColorDialog::getColor(*c2, dlg, Lang::tr("Cor 2"));
        if (chosen.isValid()) {
            *c2 = chosen;
            lumen::design::StyleSheet::apply(btn2, QString("background: %1; border: 2px solid rgba(255,255,255,0.3); border-radius: 6px; color: white;").arg(c2->name()));
            updatePreview();
        }
    });
    colorRow->addWidget(btn2);
    colorRow->addStretch();
    layout->addLayout(colorRow);

    // Optional cover image (overrides the gradient when set)
    QString *imagePath = new QString(currentImage);
    auto *imageRow = new QHBoxLayout();
    imageRow->setSpacing(8);

    auto *imgPreview = new QLabel();
    imgPreview->setFixedSize(50, 32);
    lumen::design::StyleSheet::apply(imgPreview, QString("background: %1; border-radius: 6px;").arg(Theme::bg().name()));
    if (!currentImage.isEmpty()) {
        QPixmap pm = Theme::roundedCover(currentImage, 50, 32, 6);
        if (!pm.isNull()) imgPreview->setPixmap(pm);
    }
    imageRow->addWidget(imgPreview);

    auto *pickImageBtn = new QPushButton(Lang::tr("Escolher imagem"));
    pickImageBtn->setFont(Theme::bodyFont(11));
    pickImageBtn->setFixedHeight(32);
    pickImageBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(pickImageBtn, QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 8px; padding: 0 10px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.05); }"
    ).arg(Theme::textSoft().name(), Theme::border().name()));
    connect(pickImageBtn, &QPushButton::clicked, [dlg, imagePath, imgPreview]() {
        QString file = QFileDialog::getOpenFileName(dlg, Lang::tr("Escolher imagem da capa"), QString(),
            Lang::tr("Imagens (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (file.isEmpty()) return;
        *imagePath = file;
        QPixmap pm = Theme::roundedCover(file, 50, 32, 6);
        if (!pm.isNull()) imgPreview->setPixmap(pm);
    });
    imageRow->addWidget(pickImageBtn);

    auto *clearImageBtn = new QPushButton(Lang::tr("Remover"));
    clearImageBtn->setFont(Theme::bodyFont(11));
    clearImageBtn->setFixedHeight(32);
    clearImageBtn->setCursor(Qt::PointingHandCursor);
    clearImageBtn->setToolTip(Lang::tr("Voltar a usar o gradiente de cores"));
    lumen::design::StyleSheet::apply(clearImageBtn, QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 8px; padding: 0 10px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.05); }"
    ).arg(Theme::textSoft().name(), Theme::border().name()));
    connect(clearImageBtn, &QPushButton::clicked, [imagePath, imgPreview]() {
        imagePath->clear();
        imgPreview->setPixmap(QPixmap());
        lumen::design::StyleSheet::apply(imgPreview, QString("background: %1; border-radius: 6px;").arg(Theme::bg().name()));
    });
    imageRow->addWidget(clearImageBtn);
    imageRow->addStretch();
    layout->addLayout(imageRow);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(Lang::tr("Cancelar"));
    cancelBtn->setFont(Theme::bodyFont(12));
    cancelBtn->setFixedHeight(34);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(cancelBtn, QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 17px; padding: 0 14px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.05); }"
    ).arg(Theme::textSoft().name(), Theme::border().name()));
    connect(cancelBtn, &QPushButton::clicked, [dlg, c1, c2, imagePath]() { delete c1; delete c2; delete imagePath; dlg->reject(); });
    btnRow->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton(Lang::tr("Salvar"));
    saveBtn->setFont(Theme::bodyFont(12));
    saveBtn->setFixedHeight(34);
    saveBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(saveBtn, QString(
        "QPushButton { background: %1; color: %2; border: none; border-radius: 17px; padding: 0 18px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }"
    ).arg(Theme::accent().name(), Theme::bg().name(), Theme::accent().lighter(110).name()));
    connect(saveBtn, &QPushButton::clicked, [this, dlg, id, c1, c2, imagePath, currentImage]() {
        m_model->updatePlaylistCover(id, *c1, *c2);
        // Only touch the image when it actually changed, to avoid re-copying
        // the already-stored file on every save.
        if (*imagePath != currentImage)
            m_model->updatePlaylistCoverImage(id, *imagePath);
        delete c1; delete c2; delete imagePath;
        dlg->accept();
    });
    btnRow->addWidget(saveBtn);
    layout->addLayout(btnRow);

    dlg->exec();
}

void FoldersPage::showDeleteConfirm(int id, const QString &name) {
    auto *dlg = new QMessageBox(this);
    dlg->setWindowTitle(Lang::tr("Excluir Playlist"));
    dlg->setText(QString(Lang::tr("Excluir a playlist \"%1\"?")).arg(name));
    dlg->setInformativeText(Lang::tr("As músicas não serão apagadas — ficarão como músicas avulsas."));
    dlg->setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    dlg->setDefaultButton(QMessageBox::Cancel);
    lumen::design::StyleSheet::apply(dlg, QString(
        "QMessageBox { background: %1; color: %2; }"
        "QLabel { color: %2; background: transparent; }"
        "QPushButton { background: %3; color: %2; border: 1px solid %4; border-radius: 8px; padding: 6px 16px; min-width: 70px; }"
        "QPushButton:hover { background: %5; }"
    ).arg(Theme::surface().name(), Theme::text().name(), Theme::card().name(),
          Theme::border().name(), Theme::cardHover().name()));

    if (dlg->exec() == QMessageBox::Yes) {
        m_model->deletePlaylist(id);
    }
}
