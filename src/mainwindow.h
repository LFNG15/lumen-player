#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include "trackmodel.h"
#include "playerbar.h"
#include "homepage.h"
#include "addmusicpage.h"
#include "folderspage.h"
#include "folderdetailpage.h"
#include "likedpage.h"
#include "queuepage.h"
#include "searchpage.h"

class QLineEdit;
class QSplitter;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

signals:
    void themeChangeRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void navigateTo(const QString &page, const QString &data = "");
    void refreshCurrentPage();
    void onTrackPlay(const Track &track);
    void showThemePicker();
    void showLanguagePicker();

private:
    QWidget *buildTopBar();
    void buildSidebar(QWidget *sidebar);
    void refreshSidebarFolders();
    void applySidebarCollapsed(bool collapsed, bool save = true);
    void updateNavButtons();
    void showToast(const QString &text);
    void repositionToast();
    void showEditTrackDialog(const Track &track);
    void confirmDeleteTrack(const Track &track);
    void showSidebarSortMenu();
    void toggleSidebarSearch();

    TrackModel *m_model;
    PlayerBar *m_playerBar;

    QStackedWidget *m_stack;
    HomePage *m_homePage;
    AddMusicPage *m_addPage;
    FoldersPage *m_foldersPage;
    FolderDetailPage *m_folderDetailPage;
    LikedPage *m_likedPage;
    QueuePage *m_queuePage = nullptr;
    SearchPage *m_searchPage;
    QLineEdit *m_searchEdit;

    // Sidebar
    QSplitter *m_splitter = nullptr;
    QWidget *m_sidebar = nullptr;
    QPushButton *m_navHome;
    QPushButton *m_navAdd;
    QPushButton *m_navFolders;
    QVBoxLayout *m_sidebarFoldersLayout;
    QWidget *m_sidebarFoldersContainer;
    QLabel *m_trackCountLabel;
    QLabel *m_logoText = nullptr;
    QLabel *m_badge = nullptr;
    QLabel *m_foldersHeader = nullptr;
    QWidget *m_foldersHeaderRow = nullptr;
    QLineEdit *m_sidebarSearchEdit = nullptr;
    QPushButton *m_sidebarSearchBtn = nullptr;
    QString m_sidebarFilter;
    QPushButton *m_collapseBtn = nullptr;
    QPushButton *m_langBtn = nullptr;
    QHBoxLayout *m_logoLayout = nullptr;
    bool m_sidebarCollapsed = false;

    QString m_currentPage;

    QLabel *m_toast = nullptr;
    QTimer *m_toastTimer = nullptr;
};

#endif // MAINWINDOW_H