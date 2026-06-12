#include <QApplication>
#include <QSettings>
#include "mainwindow.h"
#include "theme.h"
#include "lang.h"
#include <QIcon>

static const int RESTART_CODE = 1000;

int main(int argc, char *argv[])
{
    int exitCode;
    do {
        QApplication app(argc, argv);
        // Storage identifiers kept for compatibility with existing data;
        // the visible name is Lumen Music.
        app.setApplicationName("Vinil Player");
        app.setOrganizationName("VinilPlayer");
        app.setApplicationDisplayName("Lumen Music");
        app.setWindowIcon(QIcon(":/icon.png"));

        QSettings settings;
        Theme::setActiveTheme(Theme::themeById(settings.value("theme", "lumen").toString()));
        Lang::setActiveLang(settings.value("language", "pt").toString());

        app.setStyleSheet(Theme::globalStyleSheet());

        QFont defaultFont("Segoe UI", 12);
        defaultFont.setWeight(QFont::Medium);
        app.setFont(defaultFont);

        MainWindow window;
        QObject::connect(&window, &MainWindow::themeChangeRequested,
                         []() { qApp->exit(RESTART_CODE); });
        window.show();

        exitCode = app.exec();
    } while (exitCode == RESTART_CODE);

    return exitCode;
}
