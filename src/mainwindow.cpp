#include "design/stylesheet.h"
#include "design/thememanager.h"
#include "design/i18n.h"
#include "design/palettes.h"
#include "mainwindow.h"
#include "lang.h"
#include "theme.h"
#include <QShowEvent>
#include <QEvent>
#include <QApplication>
#include <QLabel>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include <QDialog>
#include <QGridLayout>
#include <QSettings>
#include <QResizeEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QIcon>
#include <QSplitter>
#include <QCloseEvent>
#include <QMenu>
#include <QButtonGroup>
#include <QRadioButton>
#include <QCheckBox>
#include <algorithm>
#include "coverwidget.h"
#include "textutils.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Lumen Music");
    resize(1100, 720);
    setMinimumSize(720, 480);

    m_model = new TrackModel(this);

    // ── Central widget ──────────────────────────────────────
    auto *central = new QWidget(this);
    setCentralWidget(central);
    lumen::design::StyleSheet::apply(central, Theme::globalStyleSheet());

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Body: a splitter so the user can resize the sidebar and queue panel.
    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(2);
    lumen::design::StyleSheet::apply(m_splitter, QString(
        "QSplitter::handle { background: %1; }"
        "QSplitter::handle:hover { background: %2; }"
    ).arg(Theme::border().name(), Theme::accentRgba(0.5)));

    // ── Sidebar ─────────────────────────────────────────────
    m_sidebar = new QWidget();
    m_sidebar->setObjectName(QStringLiteral("lumenSidebar"));
    lumen::design::StyleSheet::apply(m_sidebar, QString("background-color: %1;").arg(Theme::surface().name()));
    buildSidebar(m_sidebar);
    m_splitter->addWidget(m_sidebar);

    // ── Stacked pages ───────────────────────────────────────
    m_stack = new QStackedWidget();
    lumen::design::StyleSheet::apply(m_stack, "background: transparent;");

    m_homePage = new HomePage(m_model, this);
    m_addPage = new AddMusicPage(m_model, this);
    m_foldersPage = new FoldersPage(m_model, this);
    m_folderDetailPage = new FolderDetailPage(m_model, this);
    m_likedPage = new LikedPage(m_model, this);
    m_searchPage = new SearchPage(m_model, this);

    m_stack->addWidget(m_homePage);       // 0
    m_stack->addWidget(m_addPage);        // 1
    m_stack->addWidget(m_foldersPage);    // 2
    m_stack->addWidget(m_folderDetailPage); // 3
    m_stack->addWidget(m_likedPage);      // 4

    // Content column: global search bar on top of the pages.
    auto *contentWidget = new QWidget();
    lumen::design::StyleSheet::apply(contentWidget, "background: transparent;");
    contentWidget->setMinimumWidth(340);
    auto *contentColumn = new QVBoxLayout(contentWidget);
    contentColumn->setContentsMargins(0, 0, 0, 0);
    contentColumn->setSpacing(0);
    contentColumn->addWidget(buildTopBar());
    contentColumn->addWidget(m_stack, 1);

    m_splitter->addWidget(contentWidget);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(m_splitter, 1);

    // ── Playback engine + player bar (view) ─────────────────
    m_engine = new PlaybackEngine(m_model, this);
    m_playerBar = new PlayerBar(m_engine, this);
    mainLayout->addWidget(m_playerBar);

    m_stack->addWidget(m_searchPage);  // 5

    // Queue side panel (Spotify-style): lives next to the content, outside
    // the stack, and is toggled by the player bar's queue button. It needs
    // the player bar to read the live queue.
    m_queuePage = new QueuePage(m_model, m_playerBar, this);
    m_queuePage->setMinimumWidth(180);
    m_queuePage->setMaximumWidth(480);
    m_queuePage->hide();
    m_splitter->addWidget(m_queuePage);
    m_splitter->setStretchFactor(2, 0);

    // Restore the user's panel sizes and collapsed state from the last session.
    {
        QSettings settings;
        applySidebarCollapsed(settings.value("sidebarCollapsed", false).toBool(), false);
        const int sidebarW = settings.value("sidebarWidth", 260).toInt();
        if (!m_sidebarCollapsed)
            m_splitter->setSizes({sidebarW, qMax(360, width() - sidebarW)});
    }
    connect(m_splitter, &QSplitter::splitterMoved, this, [this]() {
        QSettings settings;
        if (!m_sidebarCollapsed)
            settings.setValue("sidebarWidth", m_splitter->sizes().value(0));
        if (m_queuePage->isVisible())
            settings.setValue("queueWidth", m_splitter->sizes().value(2));
        // The grid view sizes its cells from the sidebar width.
        if (!m_sidebarCollapsed && settings.value("sidebarView", "list").toString() == "grid")
            refreshSidebarFolders();
    });

    // Live theme + language (P1) — polish chrome and rebuild visible pages.
    connect(&lumen::design::ThemeManager::instance(), &lumen::design::ThemeManager::changed,
            this, &MainWindow::onDesignChanged);
    connect(&lumen::design::LanguageManager::instance(), &lumen::design::LanguageManager::changed,
            this, &MainWindow::onDesignChanged);

    // ── Connections ─────────────────────────────────────────
    // Home page
    connect(m_homePage, &HomePage::playRequested, this, &MainWindow::onTrackPlay);
    connect(m_homePage, &HomePage::likeToggled, this, [this](int id) {
        m_model->toggleLike(id);
        refreshCurrentPage();
    });
    connect(m_homePage, &HomePage::navigateTo, this, &MainWindow::navigateTo);
    connect(m_homePage, &HomePage::enqueueRequested, this, [this](const Track &t) {
        m_playerBar->enqueue(t);
        showToast(Lang::tr("Adicionado à fila"));
    });
    connect(m_homePage, &HomePage::editTrackRequested, this, [this](const Track &t) {
        showEditTrackDialog(t);
    });
    connect(m_homePage, &HomePage::deleteRequested, this, [this](int id) {
        if (Track *t = m_model->findTrack(id)) confirmDeleteTrack(*t);
    });

    // Add page
    connect(m_addPage, &AddMusicPage::trackAdded, this, [this](const Track &) {
        refreshCurrentPage();
        refreshSidebarFolders();
    });
    connect(m_addPage, &AddMusicPage::navigateBack, this, [this]() { navigateTo("home"); });

    // Folders page
    connect(m_foldersPage, &FoldersPage::folderSelected, this, [this](const QString &name) {
        navigateTo("folder", name);
    });

    // Folder detail
    connect(m_folderDetailPage, &FolderDetailPage::playRequested, this, &MainWindow::onTrackPlay);
    connect(m_folderDetailPage, &FolderDetailPage::likeToggled, this, [this](int id) {
        m_model->toggleLike(id);
        refreshCurrentPage();
    });
    connect(m_folderDetailPage, &FolderDetailPage::deleteRequested, this, [this](int id) {
        if (Track *t = m_model->findTrack(id)) confirmDeleteTrack(*t);
    });
    connect(m_folderDetailPage, &FolderDetailPage::editTrackRequested, this, [this](const Track &t) {
        showEditTrackDialog(t);
    });
    connect(m_folderDetailPage, &FolderDetailPage::navigateBack, this, [this]() { navigateTo("folders"); });
    connect(m_folderDetailPage, &FolderDetailPage::enqueueRequested, this, [this](const Track &t) {
        m_playerBar->enqueue(t);
        showToast(Lang::tr("Adicionado à fila"));
    });

    // Decision 7: non-blocking owner notice when a track is referenced by
    // another playlist (file is never copied or moved).
    connect(m_model, &TrackModel::ownerNotice, this, [this](const AddToPlaylistResult &r) {
        QSettings s;
        if (s.value(QStringLiteral("notices/ownerExplained"), false).toBool()
            && !r.crossesOwner) {
            return; // first-owner toast suppressible after user dismisses once
        }
        QString msg;
        if (r.crossesOwner) {
            msg = Lang::tr("«%1» foi adicionada a %2. O arquivo não foi duplicado — ele permanece na pasta de %3.")
                      .arg(r.trackTitle, r.destName, r.ownerName);
        } else if (r.firstOwner) {
            msg = Lang::tr("«%1» foi adicionada a %2. O arquivo permanece na pasta geral do Lumen Music.")
                      .arg(r.trackTitle, r.destName);
        } else {
            return;
        }
        showToast(msg);
        // Remember that the user has seen the explanation.
        s.setValue(QStringLiteral("notices/ownerExplained"), true);
    });

    // Liked page
    connect(m_likedPage, &LikedPage::playRequested, this, &MainWindow::onTrackPlay);
    connect(m_likedPage, &LikedPage::likeToggled, this, [this](int id) {
        m_model->toggleLike(id);
        refreshCurrentPage();
    });
    connect(m_likedPage, &LikedPage::navigateBack, this, [this]() { navigateTo("home"); });
    connect(m_likedPage, &LikedPage::navigateToFolder, this, [this](const QString &name) {
        navigateTo("folder", name);
    });
    connect(m_likedPage, &LikedPage::enqueueRequested, this, [this](const Track &t) {
        m_playerBar->enqueue(t);
        showToast(Lang::tr("Adicionado à fila"));
    });
    connect(m_likedPage, &LikedPage::editTrackRequested, this, [this](const Track &t) {
        showEditTrackDialog(t);
    });
    connect(m_likedPage, &LikedPage::deleteRequested, this, [this](int id) {
        if (Track *t = m_model->findTrack(id)) confirmDeleteTrack(*t);
    });

    // Queue page
    connect(m_queuePage, &QueuePage::playContext, this, [this](const Track &t) {
        m_playerBar->playKeepingContext(t);
        refreshCurrentPage();
    });
    connect(m_queuePage, &QueuePage::playFromQueue, this, [this](int index) {
        Track t;
        if (m_playerBar->takeFromQueue(index, t)) {
            m_playerBar->playKeepingContext(t);
        }
        refreshCurrentPage();
    });
    connect(m_queuePage, &QueuePage::removeFromQueueRequested, this, [this](int index) {
        m_playerBar->removeFromQueue(index);
    });
    connect(m_queuePage, &QueuePage::navigateBack, this, [this]() { m_queuePage->hide(); });

    // Search page
    connect(m_searchPage, &SearchPage::playRequested, this, &MainWindow::onTrackPlay);
    connect(m_searchPage, &SearchPage::likeToggled, this, [this](int id) {
        m_model->toggleLike(id);
        refreshCurrentPage();
    });
    connect(m_searchPage, &SearchPage::enqueueRequested, this, [this](const Track &t) {
        m_playerBar->enqueue(t);
        showToast(Lang::tr("Adicionado à fila"));
    });
    connect(m_searchPage, &SearchPage::navigateTo, this, &MainWindow::navigateTo);

    // Player bar
    connect(m_playerBar, &PlayerBar::trackChanged, this, [this](int) {
        refreshCurrentPage();
    });
    connect(m_playerBar, &PlayerBar::queueRequested, this, [this]() {
        if (m_queuePage->isVisible()) {
            m_queuePage->hide();
        } else {
            m_queuePage->refresh(m_playerBar->currentTrackId(), m_playerBar->isPlaying());
            m_queuePage->show();
            // Responsive default width: ~28% of window, clamped; restore last drag.
            const int winW = qMax(720, width());
            const int defW = qBound(200, winW / 4 + 40, 360);
            int queueW = QSettings().value("queueWidth", defW).toInt();
            queueW = qBound(180, queueW, qMin(480, winW / 2));
            m_queuePage->setMinimumWidth(180);
            m_queuePage->setMaximumWidth(qMin(480, qMax(220, winW / 2)));
            auto sizes = m_splitter->sizes();
            if (sizes.size() == 3) {
                const int total = sizes[0] + sizes[1] + sizes[2];
                const int contentMin = qMax(280, winW / 3);
                queueW = qMin(queueW, total - sizes[0] - contentMin);
                queueW = qMax(180, queueW);
                m_splitter->setSizes({sizes[0], total - sizes[0] - queueW, queueW});
            }
        }
    });
    connect(m_playerBar, &PlayerBar::queueChanged, this, [this]() { refreshCurrentPage(); });

    // Model changes — keep the count label, the visible page, and the sidebar
    // in sync so edits (cover image/colors, rename, etc.) reflect immediately.
    connect(m_model, &TrackModel::tracksChanged, this, [this]() {
        m_trackCountLabel->setText(QString(Lang::tr("%1 faixa%2 na biblioteca"))
            .arg(m_model->tracks().size())
            .arg(m_model->tracks().size() != 1 ? "s" : ""));
        refreshCurrentPage();
        refreshSidebarFolders();
    });

    // Persist the playback session on quit — aboutToQuit also fires on the
    // theme/language restart path, which skips closeEvent.
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        m_playerBar->persistState();
        if (m_nowPlaying)
            m_nowPlaying->setEnabled(false);
    });

    // Resume the last session: same track, same position, paused.
    m_playerBar->restoreSession();

    // Initial state
    navigateTo("home");
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // SMTC needs a real HWND — only available after the window is shown.
    if (!m_platformReady)
        setupPlatformIntegration();
}

void MainWindow::setupPlatformIntegration()
{
    m_platformReady = true;

    // --- SMTC (or null) ---
    m_nowPlaying = lumen::platform::NowPlaying::create(this);
    if (m_nowPlaying) {
        connect(m_nowPlaying.get(), &lumen::platform::NowPlaying::commandReceived,
                this, &MainWindow::onNowPlayingCommand);

        connect(m_engine, &PlaybackEngine::trackChanged, this, [this](int) {
            pushNowPlayingMetadata();
        });
        connect(m_engine, &PlaybackEngine::playingChanged, this, [this](bool playing) {
            if (m_nowPlaying)
                m_nowPlaying->setPlaybackState(playing);
        });
        connect(m_engine, &PlaybackEngine::positionChanged, this, [this](qint64 pos) {
            if (m_nowPlaying)
                m_nowPlaying->setTimeline(pos, m_engine->duration());
        });
        connect(m_engine, &PlaybackEngine::durationChanged, this, [this](qint64 dur) {
            if (m_nowPlaying)
                m_nowPlaying->setTimeline(m_engine->position(), dur);
        });
        connect(m_engine, &PlaybackEngine::queueChanged, this, [this]() {
            if (!m_nowPlaying) return;
            m_nowPlaying->setCanNext(true);
            m_nowPlaying->setCanPrevious(true);
        });

        m_nowPlaying->setEnabled(true);
        if (m_engine->currentTrackId() != 0)
            pushNowPlayingMetadata();
        m_nowPlaying->setPlaybackState(m_engine->isPlaying());
    }

    // --- Media keys fallback when SMTC is unavailable ---
    m_mediaKeys = new lumen::platform::MediaKeys(this);
    if (!m_nowPlaying || !m_nowPlaying->isAvailable()) {
        m_mediaKeys->install();
        connect(m_mediaKeys, &lumen::platform::MediaKeys::playPause,
                m_engine, &PlaybackEngine::togglePlay);
        connect(m_mediaKeys, &lumen::platform::MediaKeys::next,
                m_engine, &PlaybackEngine::next);
        connect(m_mediaKeys, &lumen::platform::MediaKeys::previous,
                m_engine, &PlaybackEngine::prev);
        connect(m_mediaKeys, &lumen::platform::MediaKeys::stop,
                m_engine, &PlaybackEngine::stop);
    }

    // --- System tray ---
    m_tray = new lumen::platform::TrayIcon(m_engine, this);
    if (m_tray->isAvailable()) {
        m_tray->show();
        connect(m_tray, &lumen::platform::TrayIcon::showMainWindow, this, [this]() {
            showNormal();
            raise();
            activateWindow();
        });
        connect(m_tray, &lumen::platform::TrayIcon::quitRequested, this, [this]() {
            m_minimizeToTray = false;
            close();
        });
    }
}

void MainWindow::pushNowPlayingMetadata()
{
    if (!m_nowPlaying || !m_engine) return;
    const Track t = m_engine->currentTrack();
    if (t.id == 0) {
        m_nowPlaying->setEnabled(false);
        return;
    }
    m_nowPlaying->setEnabled(true);
    lumen::platform::NowPlayingInfo info;
    info.title = t.title;
    info.artist = t.artist;
    info.album = t.folder;
    info.color1 = t.cover.c1;
    info.color2 = t.cover.c2;
    info.durationMs = t.durationMs > 0 ? t.durationMs : m_engine->duration();
    m_nowPlaying->setMetadata(info);
    m_nowPlaying->setPlaybackState(m_engine->isPlaying());
    m_nowPlaying->setTimeline(m_engine->position(), info.durationMs);
    m_nowPlaying->setCanNext(true);
    m_nowPlaying->setCanPrevious(true);
}

void MainWindow::onNowPlayingCommand(lumen::platform::TransportCommand cmd, qint64 argMs)
{
    if (!m_engine) return;
    using TC = lumen::platform::TransportCommand;
    switch (cmd) {
    case TC::Play:
        if (!m_engine->isPlaying()) m_engine->togglePlay();
        break;
    case TC::Pause:
        if (m_engine->isPlaying()) m_engine->togglePlay();
        break;
    case TC::Toggle:
        m_engine->togglePlay();
        break;
    case TC::Next:
        m_engine->next();
        break;
    case TC::Previous:
        m_engine->prev();
        break;
    case TC::Stop:
        m_engine->stop();
        break;
    case TC::Seek:
        m_engine->seek(argMs);
        break;
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange
        && isMinimized()
        && m_minimizeToTray
        && m_tray
        && m_tray->isAvailable()) {
        hide();
        event->ignore();
        return;
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    m_playerBar->persistState();
    if (m_nowPlaying)
        m_nowPlaying->setEnabled(false);
    QMainWindow::closeEvent(event);
}

QWidget *MainWindow::buildTopBar() {
    auto *bar = new QWidget();
    bar->setFixedHeight(56);
    lumen::design::StyleSheet::apply(bar, "background: transparent;");

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(32, 10, 32, 6);
    layout->setSpacing(0);

    // Search pill: glyph + borderless line edit inside a rounded container.
    auto *pill = new QWidget();
    pill->setObjectName("searchPill");
    pill->setFixedHeight(40);
    pill->setMaximumWidth(440);
    lumen::design::StyleSheet::apply(pill, QString(
        "QWidget#searchPill { background: %1; border: 1px solid %2; border-radius: 20px; }"
    ).arg(Theme::surface().name(), Theme::border().name()));

    auto *pillLayout = new QHBoxLayout(pill);
    pillLayout->setContentsMargins(14, 0, 14, 0);
    pillLayout->setSpacing(8);

    auto *searchIcon = new QLabel("");
    searchIcon->setFont(Theme::iconFont(13));
    lumen::design::StyleSheet::apply(searchIcon, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    pillLayout->addWidget(searchIcon);

    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(Lang::tr("O que você quer ouvir?"));
    m_searchEdit->setFont(Theme::bodyFont(12));
    m_searchEdit->setClearButtonEnabled(true);
    lumen::design::StyleSheet::apply(m_searchEdit, QString(
        "QLineEdit { background: transparent; color: %1; border: none; }"
    ).arg(Theme::text().name()));
    pillLayout->addWidget(m_searchEdit, 1);

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_searchPage->setQuery(text);
        if (!text.trimmed().isEmpty()) {
            if (m_currentPage != "search") navigateTo("search");
            else refreshCurrentPage();
        } else if (m_currentPage == "search") {
            navigateTo("home");
        }
    });

    layout->addStretch();
    layout->addWidget(pill, 1);
    layout->addStretch();
    return bar;
}

void MainWindow::buildSidebar(QWidget *sidebar) {
    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Logo
    auto *logoWidget = new QWidget();
    lumen::design::StyleSheet::apply(logoWidget, "background: transparent;");
    m_logoLayout = new QHBoxLayout(logoWidget);
    m_logoLayout->setContentsMargins(20, 20, 20, 16);
    m_logoLayout->setSpacing(8);

    auto *logoIcon = new QLabel();
    logoIcon->setFixedSize(30, 30);
    logoIcon->setScaledContents(true);
    lumen::design::StyleSheet::apply(logoIcon, "background: transparent;");
    logoIcon->setPixmap(QIcon(":/icon.png").pixmap(30, 30));
    m_logoLayout->addWidget(logoIcon);

    m_logoText = new QLabel("Lumen");
    m_logoText->setFont(Theme::titleFont(18));
    lumen::design::StyleSheet::apply(m_logoText, QString("color: %1; background: transparent;").arg(Theme::text().name()));
    m_logoLayout->addWidget(m_logoText);

    m_badge = new QLabel("MUSIC");
    m_badge->setFont(Theme::bodyFont(9));
    QColor ac = Theme::accent();
    lumen::design::StyleSheet::apply(m_badge, QString("color: %1; background: rgba(%2,%3,%4,0.16); border-radius: 4px; padding: 2px 6px; font-weight: bold; letter-spacing: 1px;")
        .arg(Theme::accent().name()).arg(ac.red()).arg(ac.green()).arg(ac.blue()));
    m_logoLayout->addWidget(m_badge);
    m_logoLayout->addStretch();

    // Collapse/expand the sidebar (icons + covers only when collapsed).
    m_collapseBtn = new QPushButton("");
    m_collapseBtn->setFixedSize(24, 24);
    m_collapseBtn->setCursor(Qt::PointingHandCursor);
    m_collapseBtn->setFont(Theme::iconFont(10));
    lumen::design::StyleSheet::apply(m_collapseBtn, QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 12px; }"
        "QPushButton:hover { background: " + Theme::hoverBg(0.08) + "; color: %2; }"
    ).arg(Theme::textMuted().name(), Theme::text().name()));
    connect(m_collapseBtn, &QPushButton::clicked, this, [this]() {
        applySidebarCollapsed(!m_sidebarCollapsed);
    });
    m_logoLayout->addWidget(m_collapseBtn);

    layout->addWidget(logoWidget);

    // Navigation buttons
    auto *navWidget = new QWidget();
    lumen::design::StyleSheet::apply(navWidget, "background: transparent;");
    auto *navLayout = new QVBoxLayout(navWidget);
    navLayout->setContentsMargins(10, 0, 10, 0);
    navLayout->setSpacing(2);

    auto makeNavBtn = [this](const QString &text, const QString &icon) -> QPushButton* {
        auto *btn = new QPushButton(QString("  %1  %2").arg(icon, text));
        btn->setObjectName(QStringLiteral("lumenNavItem"));
        btn->setFixedHeight(40);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFont(Theme::bodyFont(13));
        // Stash the parts so the collapsed mode can show the icon alone.
        btn->setProperty("navIcon", icon);
        btn->setProperty("navText", text);
        // Idle soft; hover/active use solid accent + onAccent (white on deep accents).
        lumen::design::StyleSheet::apply(btn, QString(
            "QPushButton { background: transparent; color: %1; border: none; border-radius: 10px; "
            "text-align: left; padding-left: 14px; font-family: \"Segoe UI\", \"Segoe MDL2 Assets\"; font-weight: 600; }"
            "QPushButton:hover { background-color: %2; color: %3; }"
        ).arg(Theme::textSoft().name(), Theme::accent().name(), Theme::onAccent().name()));
        return btn;
    };

    m_navHome    = makeNavBtn(Lang::tr("Início"),    "\uE10F");
    m_navAdd     = makeNavBtn(Lang::tr("Adicionar"), "\uE109");
    m_navFolders = makeNavBtn(Lang::tr("Playlists"), "\uE188");

    connect(m_navHome, &QPushButton::clicked, [this]() { navigateTo("home"); });
    connect(m_navAdd, &QPushButton::clicked, [this]() { navigateTo("add"); });
    connect(m_navFolders, &QPushButton::clicked, [this]() { navigateTo("folders"); });

    navLayout->addWidget(m_navHome);
    navLayout->addWidget(m_navAdd);
    navLayout->addWidget(m_navFolders);

    layout->addWidget(navWidget);

    // Sidebar folders header: label + search toggle + sort/view menu,
    // like Spotify's "Your Library" header.
    m_foldersHeaderRow = new QWidget();
    lumen::design::StyleSheet::apply(m_foldersHeaderRow, "background: transparent;");
    auto *headerLayout = new QHBoxLayout(m_foldersHeaderRow);
    headerLayout->setContentsMargins(20, 16, 14, 4);
    headerLayout->setSpacing(2);

    m_foldersHeader = new QLabel(Lang::tr("SUAS PLAYLISTS"));
    m_foldersHeader->setFont(Theme::bodyFont(10));
    lumen::design::StyleSheet::apply(m_foldersHeader, QString("color: %1; background: transparent; font-weight: bold; letter-spacing: 1px;")
        .arg(Theme::textMuted().name()));
    headerLayout->addWidget(m_foldersHeader);
    headerLayout->addStretch();

    auto makeHeaderBtn = [](const QString &glyph, const QString &tip) -> QPushButton* {
        auto *btn = new QPushButton(glyph);
        btn->setFixedSize(24, 24);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFont(Theme::iconFont(11));
        btn->setToolTip(tip);
        lumen::design::StyleSheet::apply(btn, QString(
            "QPushButton { background: transparent; color: %1; border: none; border-radius: 12px; }"
            "QPushButton:hover { background: " + Theme::hoverBg(0.08) + "; color: %2; }"
        ).arg(Theme::textMuted().name(), Theme::text().name()));
        return btn;
    };

    m_sidebarSearchBtn = makeHeaderBtn("", Lang::tr("Buscar playlists"));
    connect(m_sidebarSearchBtn, &QPushButton::clicked, this, &MainWindow::toggleSidebarSearch);
    headerLayout->addWidget(m_sidebarSearchBtn);

    auto *sortBtn = makeHeaderBtn("", Lang::tr("Ordenar e exibir"));
    connect(sortBtn, &QPushButton::clicked, this, &MainWindow::showSidebarSortMenu);
    headerLayout->addWidget(sortBtn);

    layout->addWidget(m_foldersHeaderRow);

    // Inline playlist search, hidden until the magnifier is clicked.
    m_sidebarSearchEdit = new QLineEdit();
    m_sidebarSearchEdit->setPlaceholderText(Lang::tr("Buscar playlists"));
    m_sidebarSearchEdit->setFont(Theme::bodyFont(11));
    m_sidebarSearchEdit->setClearButtonEnabled(true);
    m_sidebarSearchEdit->setFixedHeight(30);
    lumen::design::StyleSheet::apply(m_sidebarSearchEdit, QString(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 15px; padding: 0 12px; margin: 0 10px 4px; }"
        "QLineEdit:focus { border-color: %4; }"
    ).arg(Theme::surface().name(), Theme::text().name(),
          Theme::border().name(), Theme::accent().name()));
    m_sidebarSearchEdit->hide();
    connect(m_sidebarSearchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_sidebarFilter = text;
        refreshSidebarFolders();
    });
    layout->addWidget(m_sidebarSearchEdit);

    auto *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    lumen::design::StyleSheet::apply(scrollArea, "QScrollArea { background: transparent; border: none; }");

    m_sidebarFoldersContainer = new QWidget();
    lumen::design::StyleSheet::apply(m_sidebarFoldersContainer, "background: transparent;");
    m_sidebarFoldersLayout = new QVBoxLayout(m_sidebarFoldersContainer);
    m_sidebarFoldersLayout->setContentsMargins(10, 0, 10, 0);
    m_sidebarFoldersLayout->setSpacing(1);
    m_sidebarFoldersLayout->addStretch();

    scrollArea->setWidget(m_sidebarFoldersContainer);
    layout->addWidget(scrollArea, 1);

    // Footer
    auto *footerSep = new QFrame();
    footerSep->setFrameShape(QFrame::HLine);
    lumen::design::StyleSheet::apply(footerSep, QString("color: %1;").arg(Theme::border().name()));
    footerSep->setFixedHeight(1);
    layout->addWidget(footerSep);

    auto *footerWidget = new QWidget();
    lumen::design::StyleSheet::apply(footerWidget, "background: transparent;");
    auto *footerLayout = new QHBoxLayout(footerWidget);
    footerLayout->setContentsMargins(20, 8, 12, 12);
    footerLayout->setSpacing(4);

    m_trackCountLabel = new QLabel(QString(Lang::tr("%1 faixa%2 na biblioteca"))
        .arg(m_model->tracks().size())
        .arg(m_model->tracks().size() != 1 ? "s" : ""));
    m_trackCountLabel->setFont(Theme::bodyFont(10));
    lumen::design::StyleSheet::apply(m_trackCountLabel, QString("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    footerLayout->addWidget(m_trackCountLabel, 1);

    m_langBtn = new QPushButton(Lang::isEnglish() ? "EN" : "PT");
    m_langBtn->setFixedSize(28, 28);
    m_langBtn->setCursor(Qt::PointingHandCursor);
    m_langBtn->setFont(Theme::bodyFont(9));
    m_langBtn->setToolTip(Lang::tr("Idioma"));
    lumen::design::StyleSheet::apply(m_langBtn, QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 6px; font-weight: bold; }"
        "QPushButton:hover { background: " + Theme::hoverBg(0.08) + "; color: %2; }"
    ).arg(Theme::textMuted().name(), Theme::accent().name()));
    connect(m_langBtn, &QPushButton::clicked, this, &MainWindow::showLanguagePicker);
    footerLayout->addWidget(m_langBtn);

    auto *themeBtn = new QPushButton("\uE790");
    themeBtn->setFixedSize(28, 28);
    themeBtn->setCursor(Qt::PointingHandCursor);
    themeBtn->setFont(Theme::iconFont(12));
    themeBtn->setToolTip(Lang::tr("Escolher tema"));
    lumen::design::StyleSheet::apply(themeBtn, QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 6px; }"
        "QPushButton:hover { background: " + Theme::hoverBg(0.08) + "; color: %2; }"
    ).arg(Theme::textMuted().name(), Theme::accent().name()));
    connect(themeBtn, &QPushButton::clicked, this, &MainWindow::showThemePicker);
    footerLayout->addWidget(themeBtn);

    layout->addWidget(footerWidget);
}

void MainWindow::updateNavButtons() {
    for (auto *btn : {m_navHome, m_navAdd, m_navFolders}) {
        const QString icon = btn->property("navIcon").toString();
        const QString text = btn->property("navText").toString();
        btn->setText(m_sidebarCollapsed ? icon : QString("  %1  %2").arg(icon, text));
        btn->setToolTip(m_sidebarCollapsed ? text : QString());
    }
}

void MainWindow::applySidebarCollapsed(bool collapsed, bool save) {
    m_sidebarCollapsed = collapsed;
    if (save) QSettings().setValue("sidebarCollapsed", collapsed);

    // Text-bearing elements disappear; icons and covers stay.
    m_logoText->setVisible(!collapsed);
    m_badge->setVisible(!collapsed);
    m_foldersHeaderRow->setVisible(!collapsed);
    m_sidebarSearchEdit->setVisible(!collapsed && !m_sidebarFilter.isEmpty());
    m_trackCountLabel->setVisible(!collapsed);
    m_langBtn->setVisible(!collapsed);

    m_collapseBtn->setText(collapsed ? "" : "");
    m_collapseBtn->setToolTip(collapsed ? Lang::tr("Expandir menu") : Lang::tr("Recolher menu"));
    m_logoLayout->setContentsMargins(collapsed ? 10 : 20, 20, collapsed ? 10 : 20, 16);

    if (collapsed) {
        m_sidebar->setMinimumWidth(84);
        m_sidebar->setMaximumWidth(84);
        if (m_splitter)
            m_splitter->setSizes({84, qMax(360, width() - 84)});
    } else {
        m_sidebar->setMinimumWidth(180);
        m_sidebar->setMaximumWidth(420);
        const int sidebarW = QSettings().value("sidebarWidth", 260).toInt();
        if (m_splitter)
            m_splitter->setSizes({sidebarW, qMax(360, width() - sidebarW)});
    }

    updateNavButtons();
    navigateTo(m_currentPage.isEmpty() ? QStringLiteral("home") : m_currentPage,
               m_folderDetailPage->property("folderName").toString());
}

void MainWindow::refreshSidebarFolders() {
    // Clear
    QLayoutItem *item;
    while ((item = m_sidebarFoldersLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    auto folders = m_model->folders();

    // Live playlist search (the magnifier in the header).
    const QString needle = TextUtils::normalized(m_sidebarFilter);
    if (!needle.isEmpty()) {
        folders.erase(std::remove_if(folders.begin(), folders.end(),
            [&needle](const Folder &f) { return !TextUtils::normalized(f.name).contains(needle); }),
            folders.end());
    }

    // Sort mode, persisted like the per-playlist sort.
    const QString sortMode = QSettings().value("sidebarSort", "recents").toString();
    if (sortMode == "alpha") {
        std::sort(folders.begin(), folders.end(), [](const Folder &a, const Folder &b) {
            return TextUtils::normalized(a.name) < TextUtils::normalized(b.name);
        });
    } else if (sortMode == "added") {
        std::sort(folders.begin(), folders.end(), [](const Folder &a, const Folder &b) {
            return a.id > b.id;
        });
    } else {  // "recents": last played first; never-played keep creation order.
        QHash<int, qint64> lastPlayed;
        for (const auto &t : m_model->tracks())
            if (t.folderId > 0 && t.lastPlayedAt > lastPlayed.value(t.folderId))
                lastPlayed[t.folderId] = t.lastPlayedAt;
        std::stable_sort(folders.begin(), folders.end(), [&lastPlayed](const Folder &a, const Folder &b) {
            return lastPlayed.value(a.id) > lastPlayed.value(b.id);
        });
    }

    const QString viewMode = QSettings().value("sidebarView", "list").toString();
    auto isActiveFolder = [this](const Folder &f) {
        return m_currentPage == "folder"
            && m_folderDetailPage->property("folderName").toString() == f.name;
    };
    auto rowStyle = [](bool active) {
        if (active) {
            return QString(
                "QPushButton { background-color: %1; border: none; border-radius: 8px; color: %2; }"
                "QPushButton:hover { background-color: %3; color: %2; }"
            ).arg(Theme::accent().name(), Theme::onAccent().name(), Theme::accentHover().name());
        }
        return QString(
            "QPushButton { background: transparent; border: none; border-radius: 8px; color: %1; }"
            "QPushButton:hover { background-color: %2; color: %3; }"
        ).arg(Theme::textSoft().name(), Theme::accent().name(), Theme::onAccent().name());
    };

    if (folders.isEmpty()) {
        if (!m_sidebarCollapsed) {
            auto *emptyLabel = new QLabel(Lang::tr("Nenhuma playlist"));
            emptyLabel->setFont(Theme::bodyFont(11));
            lumen::design::StyleSheet::apply(emptyLabel, QString("color: %1; background: transparent; padding: 4px 14px;").arg(Theme::textMuted().name()));
            m_sidebarFoldersLayout->addWidget(emptyLabel);
        }
    } else if (!m_sidebarCollapsed && viewMode == "grid") {
        // Grid of covers with the name underneath, like Spotify's grid view.
        auto *gridWidget = new QWidget();
        lumen::design::StyleSheet::apply(gridWidget, "background: transparent;");
        auto *grid = new QGridLayout(gridWidget);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(6);

        const int availW = qMax(150, m_sidebar->width() - 34);
        const int cols = qMax(2, availW / 104);
        const int cellW = (availW - (cols - 1) * 6) / cols;

        for (int i = 0; i < folders.size(); ++i) {
            const Folder &f = folders[i];
            auto tracks = m_model->tracksInFolder(f.name);

            auto *btn = new QPushButton();
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFixedSize(cellW, cellW + 16);
            btn->setToolTip(f.name);

            auto *cellLayout = new QVBoxLayout(btn);
            cellLayout->setContentsMargins(6, 6, 6, 4);
            cellLayout->setSpacing(4);
            cellLayout->addWidget(CoverWidget::playlistCover(f, tracks, cellW - 12, 6), 0, Qt::AlignHCenter);

            auto *nameLabel = new QLabel(QFontMetrics(Theme::bodyFont(10))
                .elidedText(f.name, Qt::ElideRight, cellW - 12));
            nameLabel->setFont(Theme::bodyFont(10));
            lumen::design::StyleSheet::apply(nameLabel, QString("color: %1; background: transparent;").arg(Theme::textSoft().name()));
            cellLayout->addWidget(nameLabel, 0, Qt::AlignHCenter);

            lumen::design::StyleSheet::apply(btn, rowStyle(isActiveFolder(f)));
            QString folderName = f.name;
            connect(btn, &QPushButton::clicked, [this, folderName]() { navigateTo("folder", folderName); });
            grid->addWidget(btn, i / cols, i % cols);
        }
        grid->setColumnStretch(cols, 1);
        m_sidebarFoldersLayout->addWidget(gridWidget);
    } else {
        const bool compact = !m_sidebarCollapsed && viewMode == "compact";
        for (auto &f : folders) {
            auto tracks = m_model->tracksInFolder(f.name);
            auto *btn = new QPushButton();
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFixedHeight(m_sidebarCollapsed ? 48 : (compact ? 30 : 42));
            btn->setFont(Theme::bodyFont(12));

            auto *btnLayout = new QHBoxLayout(btn);
            // Small cover, like Spotify's sidebar (image > mosaic > gradient).
            // Collapsed mode shows only the cover, centered, with the name as
            // a tooltip. Compact mode drops the cover entirely.
            if (m_sidebarCollapsed) {
                btnLayout->setContentsMargins(0, 0, 0, 0);
                btnLayout->setAlignment(Qt::AlignCenter);
                btnLayout->addWidget(CoverWidget::playlistCover(f, tracks, 32, 5));
                btn->setToolTip(f.name);
            } else {
                btnLayout->setContentsMargins(8, 0, 14, 0);
                btnLayout->setSpacing(8);
                if (!compact)
                    btnLayout->addWidget(CoverWidget::playlistCover(f, tracks, 28, 5), 0, Qt::AlignVCenter);
                const bool active = isActiveFolder(f);
                // Labels don't inherit QPushButton color — set explicitly for active (white/onAccent).
                const QString nameCol = active ? Theme::onAccent().name() : Theme::textSoft().name();
                const QString countCol = active ? Theme::onAccent().name() : Theme::textMuted().name();
                auto *nameLabel = new QLabel(f.name);
                nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
                lumen::design::StyleSheet::apply(nameLabel, QString(
                    "color: %1; background: transparent;").arg(nameCol));
                nameLabel->setFont(Theme::bodyFont(12));
                auto *countLabel = new QLabel(QString::number(tracks.size()));
                countLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
                countLabel->setFont(Theme::monoFont(10));
                lumen::design::StyleSheet::apply(countLabel, QString(
                    "color: %1; background: transparent;").arg(countCol));
                btnLayout->addWidget(nameLabel);
                btnLayout->addStretch();
                btnLayout->addWidget(countLabel);
            }

            lumen::design::StyleSheet::apply(btn, rowStyle(isActiveFolder(f)));

            QString folderName = f.name;
            connect(btn, &QPushButton::clicked, [this, folderName]() { navigateTo("folder", folderName); });

            m_sidebarFoldersLayout->addWidget(btn);
        }
    }

    m_sidebarFoldersLayout->addStretch();
}

void MainWindow::toggleSidebarSearch() {
    const bool show = !m_sidebarSearchEdit->isVisible();
    m_sidebarSearchEdit->setVisible(show);
    if (show) {
        m_sidebarSearchEdit->setFocus();
    } else if (!m_sidebarFilter.isEmpty()) {
        m_sidebarSearchEdit->clear();  // textChanged refreshes the list
    }
}

void MainWindow::showSidebarSortMenu() {
    auto *menu = new QMenu(this);
    lumen::design::StyleSheet::apply(menu, QString(
        "QMenu { background: %1; border: 1px solid %2; border-radius: 8px; padding: 4px; color: %3; }"
        "QMenu::item { padding: 8px 16px; border-radius: 4px; }"
        "QMenu::item:selected { background: %4; }"
        "QMenu::item:disabled { color: %5; }"
        "QMenu::separator { height: 1px; background: %2; margin: 4px 0; }"
    ).arg(Theme::card().name(), Theme::border().name(), Theme::text().name(),
          Theme::cardHover().name(), Theme::textMuted().name()));

    QSettings settings;
    const QString currentSort = settings.value("sidebarSort", "recents").toString();
    const QString currentView = settings.value("sidebarView", "list").toString();

    menu->addAction(Lang::tr("Ordenar por"))->setEnabled(false);
    const QPair<QString, QString> sortModes[] = {
        {"recents", Lang::tr("Recentes")},
        {"added",   Lang::tr("Adicionadas recentemente")},
        {"alpha",   Lang::tr("Alfabética")},
    };
    for (const auto &m : sortModes) {
        QString label = (currentSort == m.first ? "✓ " : "   ") + m.second;
        QString id = m.first;
        menu->addAction(label, [this, id]() {
            QSettings().setValue("sidebarSort", id);
            refreshSidebarFolders();
        });
    }

    menu->addSeparator();
    menu->addAction(Lang::tr("Exibir como"))->setEnabled(false);
    const QPair<QString, QString> viewModes[] = {
        {"compact", Lang::tr("Compacta")},
        {"list",    Lang::tr("Lista")},
        {"grid",    Lang::tr("Grade")},
    };
    for (const auto &m : viewModes) {
        QString label = (currentView == m.first ? "✓ " : "   ") + m.second;
        QString id = m.first;
        menu->addAction(label, [this, id]() {
            QSettings().setValue("sidebarView", id);
            refreshSidebarFolders();
        });
    }

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void MainWindow::showEditTrackDialog(const Track &track) {
    int id = track.id;
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(Lang::tr("Editar Música"));
    dlg->setFixedSize(380, 200);
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
    layout->setSpacing(10);

    auto *titleLabel = new QLabel(Lang::tr("Nome da música"));
    titleLabel->setFont(Theme::bodyFont(12));
    layout->addWidget(titleLabel);
    auto *titleEdit = new QLineEdit(track.title);
    titleEdit->setFont(Theme::bodyFont(13));
    layout->addWidget(titleEdit);

    auto *artistLabel = new QLabel(Lang::tr("Artista"));
    artistLabel->setFont(Theme::bodyFont(12));
    layout->addWidget(artistLabel);
    auto *artistEdit = new QLineEdit(track.artist);
    artistEdit->setFont(Theme::bodyFont(13));
    layout->addWidget(artistEdit);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(Lang::tr("Cancelar"));
    cancelBtn->setFont(Theme::bodyFont(12));
    cancelBtn->setFixedHeight(36);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    lumen::design::StyleSheet::apply(cancelBtn, QString(
        "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 18px; padding: 0 16px; }"
        "QPushButton:hover { background: " + Theme::hoverBg(0.05) + "; }"
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
    ).arg(Theme::accent().name(), Theme::onAccent().name(), Theme::accentHover().name()));
    connect(saveBtn, &QPushButton::clicked, [this, dlg, titleEdit, artistEdit, id]() {
        QString title = titleEdit->text().trimmed();
        if (title.isEmpty()) return;
        m_model->updateTrack(id, title, artistEdit->text().trimmed());
        dlg->accept();
    });
    btnRow->addWidget(saveBtn);
    layout->addLayout(btnRow);

    connect(artistEdit, &QLineEdit::returnPressed, saveBtn, &QPushButton::click);
    dlg->exec();
}

void MainWindow::confirmDeleteTrack(const Track &track) {
    auto *dlg = new QMessageBox(this);
    dlg->setWindowTitle(Lang::tr("Excluir Música"));
    dlg->setText(QString(Lang::tr("Excluir \"%1\"?")).arg(track.title));
    dlg->setInformativeText(Lang::tr("A música será removida da biblioteca."));
    dlg->setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    dlg->setDefaultButton(QMessageBox::Cancel);
    lumen::design::StyleSheet::apply(dlg, QString(
        "QMessageBox { background: %1; color: %2; } QLabel { color: %2; background: transparent; }"
        "QPushButton { background: %3; color: %2; border: 1px solid %4; border-radius: 8px; padding: 6px 16px; min-width: 70px; }"
        "QPushButton:hover { background: %5; }"
    ).arg(Theme::surface().name(), Theme::text().name(), Theme::card().name(),
          Theme::border().name(), Theme::cardHover().name()));
    if (dlg->exec() == QMessageBox::Yes) {
        m_model->removeTrack(track.id);
        refreshSidebarFolders();
    }
}

void MainWindow::showToast(const QString &text) {
    if (!m_toast) {
        m_toast = new QLabel(this);
        m_toast->setAlignment(Qt::AlignCenter);
        m_toast->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_toastTimer = new QTimer(this);
        m_toastTimer->setSingleShot(true);
        connect(m_toastTimer, &QTimer::timeout, this, [this]() { if (m_toast) m_toast->hide(); });
    }
    // Light pill with dark text, like Spotify's "Added to queue".
    m_toast->setFont(Theme::bodyFont(12));
    lumen::design::StyleSheet::apply(m_toast, QString(
        "background: %1; color: %2; border-radius: 8px; padding: 10px 18px; font-weight: 600;")
        .arg(Theme::text().name(), Theme::bg().name()));
    m_toast->setText(text);
    m_toast->adjustSize();
    repositionToast();
    m_toast->show();
    m_toast->raise();
    // Respect reduce-motion: 0 ms hides immediately (still flashes once).
    const int toastMs = lumen::design::ThemeManager::motion().d(1800);
    m_toastTimer->start(toastMs > 0 ? toastMs : 1);
}

void MainWindow::repositionToast() {
    if (!m_toast) return;
    int x = (width() - m_toast->width()) / 2;
    int y = height() - m_playerBar->height() - m_toast->height() - 24;
    m_toast->move(x, y);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (m_toast && m_toast->isVisible()) repositionToast();

    // Keep content readable on narrow windows: clamp queue so the track list
    // is not crushed. Reflow page grids live (debounced).
    if (!m_splitter) return;
    const int w = width();
    if (m_queuePage && m_queuePage->isVisible()) {
        const int queueMax = qBound(200, w / 3, 480);
        const int queueMin = w < 900 ? 180 : 200;
        m_queuePage->setMaximumWidth(queueMax);
        m_queuePage->setMinimumWidth(queueMin);
        auto sizes = m_splitter->sizes();
        if (sizes.size() >= 3) {
            int q = sizes[2];
            if (q > queueMax) q = queueMax;
            if (q < queueMin) q = queueMin;
            if (q != sizes[2]) {
                const int delta = sizes[2] - q;
                sizes[2] = q;
                sizes[1] = qMax(260, sizes[1] + delta);
                m_splitter->setSizes(sizes);
            }
        }
    }
    // Content pane floor
    if (QWidget *content = m_stack ? m_stack->parentWidget() : nullptr)
        content->setMinimumWidth(w < 800 ? 280 : 340);

    // Live layout reflow: grids rebuild only when width moves enough.
    if (!m_resizeLayoutTimer) {
        m_resizeLayoutTimer = new QTimer(this);
        m_resizeLayoutTimer->setSingleShot(true);
        connect(m_resizeLayoutTimer, &QTimer::timeout, this, [this]() {
            const int wNow = width();
            if (m_lastLayoutWidth > 0 && qAbs(wNow - m_lastLayoutWidth) < 24)
                return;
            m_lastLayoutWidth = wNow;
            refreshCurrentPage();
            // Sidebar grid view sizes cells from width.
            if (!m_sidebarCollapsed
                && QSettings().value(QStringLiteral("sidebarView"), QStringLiteral("list")).toString()
                       == QLatin1String("grid")) {
                refreshSidebarFolders();
            }
        });
    }
    m_resizeLayoutTimer->start(40);
}

void MainWindow::navigateTo(const QString &page, const QString &data) {
    m_currentPage = page;

    // Nav chrome: idle = soft text; hover/active = solid accent + onAccent (white).
    auto activeStyle = [this](bool active) {
        const QString align = m_sidebarCollapsed
            ? QStringLiteral("text-align: center; padding-left: 0px;")
            : QStringLiteral("text-align: left; padding-left: 14px;");
        const QString fam = QStringLiteral(
            "font-family: \"Segoe UI\", \"Segoe MDL2 Assets\"; border: none; border-radius: 10px; ");
        if (active) {
            return QString(
                "QPushButton { background-color: %1; color: %2; %3 %4 font-weight: 700; }"
                "QPushButton:hover { background-color: %5; color: %2; }"
            ).arg(Theme::accent().name(),
                  Theme::onAccent().name(),
                  fam,
                  align,
                  Theme::accentHover().name());
        }
        return QString(
            "QPushButton { background: transparent; color: %1; %2 %3 font-weight: 600; }"
            "QPushButton:hover { background-color: %4; color: %5; }"
        ).arg(Theme::textSoft().name(),
              fam,
              align,
              Theme::accent().name(),
              Theme::onAccent().name());
    };

    lumen::design::StyleSheet::apply(m_navHome, activeStyle(page == "home"));
    lumen::design::StyleSheet::apply(m_navAdd, activeStyle(page == "add"));
    lumen::design::StyleSheet::apply(m_navFolders, activeStyle(page == "folders" || page == "folder"));

    if (page == "home") {
        m_stack->setCurrentIndex(0);
    } else if (page == "add") {
        m_addPage->refresh();
        m_stack->setCurrentIndex(1);
    } else if (page == "folders") {
        m_stack->setCurrentIndex(2);
    } else if (page == "folder") {
        m_folderDetailPage->setFolder(data);
        m_folderDetailPage->setProperty("folderName", data);
        m_stack->setCurrentIndex(3);
    } else if (page == "liked") {
        m_stack->setCurrentIndex(4);
    } else if (page == "search") {
        m_stack->setCurrentIndex(5);
    }

    refreshCurrentPage();
    refreshSidebarFolders();
}

void MainWindow::refreshCurrentPage() {
    int curId = m_playerBar->currentTrackId();
    bool playing = m_playerBar->isPlaying();

    if (m_stack->currentIndex() == 0) {
        m_homePage->refresh(curId, playing);
    } else if (m_stack->currentIndex() == 2) {
        m_foldersPage->refresh();
    } else if (m_stack->currentIndex() == 3) {
        m_folderDetailPage->refresh(curId, playing);
    } else if (m_stack->currentIndex() == 4) {
        m_likedPage->refresh(curId, playing);
    } else if (m_stack->currentIndex() == 5) {
        m_searchPage->refresh(curId, playing);
    }

    // The queue side panel lives outside the stack; keep it live while open.
    if (m_queuePage && m_queuePage->isVisible())
        m_queuePage->refresh(curId, playing);
}

void MainWindow::onTrackPlay(const Track &track) {
    // Build the playback queue from the context the track was launched in, so
    // each playlist plays within itself instead of the whole library.
    QList<Track> queue;
    if (m_currentPage == "folder") {
        // Use the page's displayed order so next/prev follow the sort mode.
        queue = m_folderDetailPage->displayedTracks();
    } else if (m_currentPage == "liked") {
        queue = m_model->likedTracks();
    } else {
        queue = m_model->tracks();   // home / full library
    }
    m_playerBar->playTrack(track, queue);
    refreshCurrentPage();
}

void MainWindow::onDesignChanged()
{
    reapplyChromeStyles();
    if (m_langBtn)
        m_langBtn->setText(Lang::isEnglish() ? QStringLiteral("EN") : QStringLiteral("PT"));
    refreshSidebarFolders();
    // Rebuild the visible page(s) so widget-local colors pick up new tokens.
    if (m_foldersPage)
        m_foldersPage->refresh();
    refreshCurrentPage();
    if (m_playerBar)
        m_playerBar->update();
    update();
}

void MainWindow::reapplyChromeStyles()
{
    if (centralWidget())
        lumen::design::StyleSheet::apply(centralWidget(), Theme::globalStyleSheet());
    if (m_sidebar)
        lumen::design::StyleSheet::apply(m_sidebar,
            QStringLiteral("background-color: %1;").arg(Theme::surface().name()));
    if (m_playerBar) {
        lumen::design::StyleSheet::apply(m_playerBar,
            QStringLiteral("PlayerBar { background-color: %1; border-top: 1px solid %2; }")
                .arg(Theme::surface().name(), Theme::border().name()));
    }
    if (m_logoText)
        lumen::design::StyleSheet::apply(m_logoText,
            QStringLiteral("color: %1; background: transparent;").arg(Theme::text().name()));
    if (m_trackCountLabel)
        lumen::design::StyleSheet::apply(m_trackCountLabel,
            QStringLiteral("color: %1; background: transparent;").arg(Theme::textMuted().name()));
    updateNavButtons();
}

void MainWindow::showLanguagePicker() {
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(Lang::tr("Idioma"));
    dlg->setFixedSize(280, 150);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    lumen::design::StyleSheet::apply(dlg, QString(
        "QDialog { background: %1; }"
        "QLabel  { background: transparent; color: %2; }"
    ).arg(Theme::surface().name(), Theme::text().name()));

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    auto *title = new QLabel(Lang::tr("Idioma"));
    title->setFont(Theme::titleFont(14));
    layout->addWidget(title);

    struct Option { const char *id; const char *label; };
    const Option options[] = { {"pt", "Português"}, {"en", "English"} };

    for (const auto &opt : options) {
        bool active = (Lang::activeLang() == opt.id);
        auto *btn = new QPushButton(opt.label);
        btn->setFixedHeight(38);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFont(Theme::bodyFont(12));
        lumen::design::StyleSheet::apply(btn, QString(
            "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 10px; font-weight: bold; }"
            "QPushButton:hover { border-color: %4; }"
        ).arg(active ? Theme::accentRgba(0.15) : Theme::card().name(),
              active ? Theme::accent().name() : Theme::text().name(),
              active ? Theme::accent().name() : Theme::border().name(),
              Theme::accent().name()));

        QString id = opt.id;
        connect(btn, &QPushButton::clicked, dlg, [id, dlg]() {
            // Live language switch — no restart (P1.5).
            lumen::design::LanguageManager::instance().setLang(id);
            dlg->accept();
        });
        layout->addWidget(btn);
    }

    dlg->exec();
}

void MainWindow::showThemePicker() {
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(Lang::tr("Escolher Tema"));
    dlg->setMinimumSize(380, 460);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    lumen::design::StyleSheet::apply(dlg, QString(
        "QDialog { background: %1; }"
        "QLabel  { background: transparent; color: %2; }"
    ).arg(Theme::surface().name(), Theme::text().name()));

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *title = new QLabel(Lang::tr("Paleta de Cores"));
    title->setFont(Theme::titleFont(14));
    layout->addWidget(title);

    auto *grid = new QGridLayout();
    grid->setSpacing(8);

    const auto themes   = Theme::allThemes();
    const QString curId = Theme::activeTheme().id;

    for (int i = 0; i < themes.size(); ++i) {
        const auto &t = themes[i];

        auto *btn = new QPushButton();
        btn->setFixedSize(160, 72);
        btn->setCursor(Qt::PointingHandCursor);

        bool active = (t.id == curId);
        lumen::design::StyleSheet::apply(btn, QString(
            "QPushButton {"
            "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "    stop:0 %1, stop:1 %2);"
            "  border: 2px solid %3;"
            "  border-radius: 10px;"
            "}"
            "QPushButton:hover { border: 2px solid %4; }"
        ).arg(t.bg.name(), t.accent.name(),
              active ? t.accent.name() : t.border.name(),
              t.accent.name()));

        auto *btnLayout = new QVBoxLayout(btn);
        btnLayout->setAlignment(Qt::AlignCenter);
        btnLayout->setContentsMargins(0, 0, 0, 0);

        auto *nameLabel = new QLabel(Lang::tr(t.name));
        nameLabel->setFont(Theme::bodyFont(10));
        lumen::design::StyleSheet::apply(nameLabel, QString("color: %1; background: transparent;").arg(t.text.name()));
        nameLabel->setAlignment(Qt::AlignCenter);
        btnLayout->addWidget(nameLabel);

        connect(btn, &QPushButton::clicked, dlg, [t, dlg]() {
            // Live palette switch — no restart (P1.3).
            lumen::design::ThemeManager::instance().setPaletteId(t.id);
            dlg->accept();
        });

        grid->addWidget(btn, i / 2, i % 2);
    }
    layout->addLayout(grid);

    // Mode axis (dark / light / high contrast)
    auto *modeLabel = new QLabel(Lang::tr("Modo"));
    modeLabel->setFont(Theme::bodyFont(12));
    layout->addWidget(modeLabel);
    auto *modeRow = new QHBoxLayout();
    auto *modeGroup = new QButtonGroup(dlg);
    struct ModeOpt { lumen::design::Mode mode; const char *label; };
    const ModeOpt modes[] = {
        {lumen::design::Mode::Dark, "Escuro"},
        {lumen::design::Mode::Light, "Claro"},
        {lumen::design::Mode::HighContrast, "Alto contraste"},
    };
    const auto curMode = lumen::design::ThemeManager::instance().mode();
    for (const auto &m : modes) {
        auto *rb = new QRadioButton(Lang::tr(m.label));
        rb->setFont(Theme::bodyFont(11));
        rb->setChecked(curMode == m.mode);
        modeGroup->addButton(rb);
        modeRow->addWidget(rb);
        connect(rb, &QRadioButton::toggled, dlg, [m](bool on) {
            if (on) lumen::design::ThemeManager::instance().setMode(m.mode);
        });
    }
    layout->addLayout(modeRow);

    // Density axis
    auto *densLabel = new QLabel(Lang::tr("Densidade"));
    densLabel->setFont(Theme::bodyFont(12));
    layout->addWidget(densLabel);
    auto *densRow = new QHBoxLayout();
    auto *densGroup = new QButtonGroup(dlg);
    struct DensOpt { lumen::design::Density d; const char *label; };
    const DensOpt dens[] = {
        {lumen::design::Density::Comfortable, "Confortável"},
        {lumen::design::Density::Compact, "Compacta"},
    };
    const auto curDens = lumen::design::ThemeManager::instance().density();
    for (const auto &d : dens) {
        auto *rb = new QRadioButton(Lang::tr(d.label));
        rb->setFont(Theme::bodyFont(11));
        rb->setChecked(curDens == d.d);
        densGroup->addButton(rb);
        densRow->addWidget(rb);
        connect(rb, &QRadioButton::toggled, dlg, [d](bool on) {
            if (on) lumen::design::ThemeManager::instance().setDensity(d.d);
        });
    }
    layout->addLayout(densRow);

    auto *reduce = new QCheckBox(Lang::tr("Reduzir movimento"));
    reduce->setFont(Theme::bodyFont(11));
    reduce->setChecked(lumen::design::ThemeManager::instance().reduceMotion());
    connect(reduce, &QCheckBox::toggled, dlg, [](bool on) {
        lumen::design::ThemeManager::instance().setReduceMotion(on);
    });
    layout->addWidget(reduce);

    dlg->exec();
}