#include "design/stylesheet.h"
#include "likedpage.h"
#include "lang.h"
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include "hoverplayfilter.h"

LikedPage::LikedPage(TrackModel *model, QWidget *parent)
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
    m_contentLayout->setSpacing(8);

    scroll->setWidget(content);
    outerLayout->addWidget(scroll);
}

void LikedPage::refresh(int currentTrackId, bool isPlaying) {
    QLayoutItem *item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    // Back button
    auto *backBtn = new QPushButton("\uE0A6");
    backBtn->setFixedSize(34, 34);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFont(Theme::iconFont(12));
    lumen::design::StyleSheet::apply(backBtn, QString(
        "QPushButton { background: rgba(255,255,255,0.05); color: %1; border: none; border-radius: 17px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.1); }"
    ).arg(Theme::text().name()));
    connect(backBtn, &QPushButton::clicked, this, &LikedPage::navigateBack);
    m_contentLayout->addWidget(backBtn, 0, Qt::AlignLeft);

    auto liked = m_model->likedTracks();

    // Header
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(20);

    auto *cover = new QWidget();
    cover->setFixedSize(140, 140);
    lumen::design::StyleSheet::apply(cover, QString(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 10px;"
    ).arg(Theme::accent().name(), Theme::danger().name()));
    auto *heartLabel = new QLabel("\uE00B", cover);
    heartLabel->setFont(Theme::iconFont(36));
    lumen::design::StyleSheet::apply(heartLabel, QString("color: %1; background: transparent;").arg(Theme::text().name()));
    heartLabel->setAlignment(Qt::AlignCenter);
    heartLabel->setGeometry(0, 0, 140, 140);
    headerLayout->addWidget(cover);

    auto *infoLayout = new QVBoxLayout();
    infoLayout->addStretch();

    auto *typeLabel = new QLabel(Lang::tr("COLEÇÃO"));
    typeLabel->setFont(Theme::bodyFont(10));
    lumen::design::StyleSheet::apply(typeLabel, QString("color: %1; background: transparent; font-weight: bold; letter-spacing: 1px;").arg(Theme::textMuted().name()));
    infoLayout->addWidget(typeLabel);

    auto *nameLabel = new QLabel(Lang::tr("Curtidas"));
    nameLabel->setFont(Theme::titleFont(32));
    lumen::design::StyleSheet::apply(nameLabel, QString("color: %1; background: transparent;").arg(Theme::text().name()));
    infoLayout->addWidget(nameLabel);

    auto *countLabel = new QLabel(QString(Lang::tr("%1 faixa%2"))
        .arg(liked.size()).arg(liked.size() != 1 ? "s" : ""));
    countLabel->setFont(Theme::bodyFont(12));
    lumen::design::StyleSheet::apply(countLabel, QString("color: %1; background: transparent;").arg(Theme::textSoft().name()));
    infoLayout->addWidget(countLabel);
    infoLayout->addStretch();

    headerLayout->addLayout(infoLayout, 1);

    auto *headerWidget = new QWidget();
    headerWidget->setLayout(headerLayout);
    lumen::design::StyleSheet::apply(headerWidget, "background: transparent;");
    m_contentLayout->addWidget(headerWidget);
    m_contentLayout->addSpacing(16);

    if (liked.isEmpty()) {
        auto *emptyLabel = new QLabel(Lang::tr("Nenhuma música curtida ainda"));
        emptyLabel->setFont(Theme::bodyFont(14));
        lumen::design::StyleSheet::apply(emptyLabel, QString("color: %1; background: transparent; padding-top: 20px;").arg(Theme::textMuted().name()));
        emptyLabel->setAlignment(Qt::AlignCenter);
        m_contentLayout->addWidget(emptyLabel);
        m_contentLayout->addStretch();
        return;
    }

    // Play the whole liked collection (it behaves as its own playlist).
    auto *playBtn = new QPushButton(QStringLiteral("\uE102"));
    playBtn->setFixedSize(48, 48);
    playBtn->setCursor(Qt::PointingHandCursor);
    playBtn->setFont(Theme::iconFont(16));
    playBtn->setToolTip(Lang::tr("Tocar curtidas"));
    lumen::design::StyleSheet::apply(playBtn, QString(
        "QPushButton { background: %1; color: %2; border: none; border-radius: 24px; font-family: \"Segoe MDL2 Assets\"; font-size: 16px; }"
        "QPushButton:hover { background: %3; }"
    ).arg(Theme::accent().name(), Theme::bg().name(), Theme::accent().lighter(110).name()));
    Track firstLiked = liked.first();
    connect(playBtn, &QPushButton::clicked, [this, firstLiked]() { emit playRequested(firstLiked); });
    m_contentLayout->addWidget(playBtn, 0, Qt::AlignLeft);
    m_contentLayout->addSpacing(8);

    // Track list
    for (int i = 0; i < liked.size(); ++i) {
        auto &track = liked[i];
        bool active = (track.id == currentTrackId);

        auto *row = new QWidget();
        row->setFixedHeight(52);
        row->setCursor(Qt::PointingHandCursor);
        row->setObjectName("trackRow");
        lumen::design::StyleSheet::apply(row, QString(
            "QWidget#trackRow { background: %1; border-radius: 8px; border-left: 3px solid %2; }"
        ).arg(active ? Theme::accentRgba(0.12) : QStringLiteral("transparent"),
              active ? Theme::accent().name() : "transparent"));

        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(12, 4, 12, 4);
        layout->setSpacing(12);

        QString idxNum = QString("%1").arg(i + 1, 2, 10, QChar('0'));
        auto *idx = new QLabel(active ? QStringLiteral("\uE102") : idxNum);
        idx->setFont(Theme::monoFont(12));
        idx->setFixedWidth(28);
        idx->setAlignment(Qt::AlignCenter);
        lumen::design::StyleSheet::apply(idx, QString("color: %1; background: transparent; font-family: \"Segoe MDL2 Assets\", Consolas;").arg(
            active ? Theme::accent().name() : Theme::textMuted().name()));
        idx->setAttribute(Qt::WA_TransparentForMouseEvents);  // let clicks reach the play overlay
        layout->addWidget(idx);

        auto *swatch = new QWidget();
        swatch->setFixedSize(38, 38);
        swatch->setAttribute(Qt::WA_TransparentForMouseEvents);
        lumen::design::StyleSheet::apply(swatch, QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); border-radius: 6px;")
            .arg(track.cover.c1.name(), track.cover.c2.name()));
        layout->addWidget(swatch);

        auto *infoCol = new QVBoxLayout();
        infoCol->setSpacing(1);
        auto *titleLabel = new QLabel(track.title);
        titleLabel->setFont(Theme::bodyFont(13));
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        lumen::design::StyleSheet::apply(titleLabel, QString("color: %1; background: transparent; font-weight: 600;").arg(
            active ? Theme::accent().name() : Theme::text().name()));
        auto *artistLabel = new QLabel(track.artist);
        artistLabel->setFont(Theme::bodyFont(11));
        artistLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        lumen::design::StyleSheet::apply(artistLabel, QString("color: %1; background: transparent;").arg(Theme::textSoft().name()));
        infoCol->addWidget(titleLabel);
        infoCol->addWidget(artistLabel);
        layout->addLayout(infoCol, 1);

        if (!track.folder.isEmpty()) {
            auto *tag = new QPushButton(track.folder);
            tag->setFont(Theme::bodyFont(10));
            tag->setCursor(Qt::PointingHandCursor);
            tag->setToolTip(QString(Lang::tr("Ir para a playlist \"%1\"")).arg(track.folder));
            lumen::design::StyleSheet::apply(tag, QString(
                "QPushButton { color: %1; background: " + Theme::accentRgba(0.10) + "; border: none; border-radius: 10px; padding: 2px 8px; }"
                "QPushButton:hover { background: " + Theme::accentRgba(0.28) + "; color: %2; }"
            ).arg(Theme::accentDim().name(), Theme::text().name()));
            QString folderName = track.folder;
            connect(tag, &QPushButton::clicked, [this, folderName]() { emit navigateToFolder(folderName); });
            layout->addWidget(tag);
        }

        auto *likeBtn = new QPushButton("\uE00B");
        likeBtn->setFixedSize(28, 28);
        likeBtn->setCursor(Qt::PointingHandCursor);
        lumen::design::StyleSheet::apply(likeBtn, QString("QPushButton { background: transparent; color: %1; border: none; font-size: 14px; font-family: \"Segoe MDL2 Assets\"; }").arg(Theme::accent().name()));
        int lid = track.id;
        connect(likeBtn, &QPushButton::clicked, [this, lid]() { emit likeToggled(lid); });
        layout->addWidget(likeBtn);

        auto *enqueueBtn = new QPushButton(QStringLiteral("\uE710"));
        enqueueBtn->setFixedSize(28, 28);
        enqueueBtn->setCursor(Qt::PointingHandCursor);
        enqueueBtn->setFont(Theme::iconFont(11));
        enqueueBtn->setToolTip(Lang::tr("Adicionar à fila"));
        lumen::design::StyleSheet::apply(enqueueBtn, QString(
            "QPushButton { background: transparent; color: %1; border: none; font-family: \"Segoe MDL2 Assets\"; }"
            "QPushButton:hover { color: %2; }"
        ).arg(Theme::textMuted().name(), Theme::accent().name()));
        Track eqt = track;
        connect(enqueueBtn, &QPushButton::clicked, [this, eqt]() { emit enqueueRequested(eqt); });
        layout->addWidget(enqueueBtn);

        auto *editBtn = new QPushButton(QStringLiteral("\uE70F"));
        editBtn->setFixedSize(28, 28);
        editBtn->setCursor(Qt::PointingHandCursor);
        editBtn->setFont(Theme::iconFont(11));
        editBtn->setToolTip(Lang::tr("Editar música"));
        lumen::design::StyleSheet::apply(editBtn, QString(
            "QPushButton { background: transparent; color: %1; border: none; font-family: \"Segoe MDL2 Assets\"; }"
            "QPushButton:hover { color: %2; }"
        ).arg(Theme::textMuted().name(), Theme::accent().name()));
        Track et = track;
        connect(editBtn, &QPushButton::clicked, [this, et]() { emit editTrackRequested(et); });
        layout->addWidget(editBtn);

        auto *delBtn = new QPushButton(QStringLiteral("\uE107"));
        delBtn->setFixedSize(28, 28);
        delBtn->setCursor(Qt::PointingHandCursor);
        delBtn->setFont(Theme::iconFont(11));
        delBtn->setToolTip(Lang::tr("Excluir música"));
        lumen::design::StyleSheet::apply(delBtn, QString(
            "QPushButton { background: transparent; color: %1; border: none; font-family: \"Segoe MDL2 Assets\"; }"
            "QPushButton:hover { color: %2; }"
        ).arg(Theme::textMuted().name(), Theme::danger().name()));
        int did = track.id;
        connect(delBtn, &QPushButton::clicked, [this, did]() { emit deleteRequested(did); });
        layout->addWidget(delBtn);

        auto *dur = new QLabel(Theme::formatTime(track.durationMs));
        dur->setFont(Theme::monoFont(11));
        dur->setFixedWidth(40);
        dur->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lumen::design::StyleSheet::apply(dur, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
        layout->addWidget(dur);

        Track t = track;
        auto *overlay = new QPushButton(row);
        overlay->setGeometry(0, 0, 9999, 52);
        lumen::design::StyleSheet::apply(overlay, "background: transparent; border: none;");
        overlay->setCursor(Qt::PointingHandCursor);
        overlay->lower();
        connect(overlay, &QPushButton::clicked, [this, t]() { emit playRequested(t); });
        if (!active) overlay->installEventFilter(new HoverPlayFilter(idx, idxNum, overlay));

        m_contentLayout->addWidget(row);
    }

    m_contentLayout->addStretch();
}
