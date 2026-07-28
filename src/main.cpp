#include <QApplication>
#include <QSettings>
#include <QIcon>
#include <QElapsedTimer>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QEventLoop>
#include <QTimer>
#include <QFileInfo>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QEvent>
#include <cstdio>

#include "mainwindow.h"
#include "theme.h"
#include "lang.h"
#include "database.h"

static const int RESTART_CODE = 1000;

// Reports cold-start latency on the first paint of the main window.
// (QWindow::frameSwapped is not reliably available for plain QWidget windows
// across Qt 6 kits; first Paint is the equivalent "first frame" signal.)
class StartupProbe : public QObject {
public:
    explicit StartupProbe(const QElapsedTimer &timer, QObject *parent = nullptr)
        : QObject(parent), m_timer(timer) {}

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Paint && !m_reported) {
            m_reported = true;
            const qint64 ms = m_timer.elapsed();
            qInfo() << "Cold start (main → first paint):" << ms << "ms"
                    << (ms < 1500 ? "(within 1.5s budget)" : "(OVER 1.5s budget)");
            watched->removeEventFilter(this);
            deleteLater();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QElapsedTimer m_timer;
    bool m_reported = false;
};

// --- P0.b helpers -----------------------------------------------------------

// Assert canary: with lang == "en", a known Portuguese key must translate.
// Without /utf-8 under MSVC the key bytes are wrong, the hash miss silently,
// and Lang::tr returns the Portuguese source string unchanged.
static void assertI18nCanary()
{
#if defined(QT_DEBUG) || defined(LUMEN_FORCE_I18N_CANARY)
    if (Lang::isEnglish()) {
        const QString key = QStringLiteral("Músicas");
        const QString out = Lang::tr(key);
        if (out == key) {
            qCritical() << "I18N CANARY FAILED: Lang::tr(\"Músicas\") returned the "
                           "Portuguese key unchanged under lang=en. "
                           "Likely missing /utf-8 on MSVC — see ContextProject.md §7.3.";
            std::fprintf(stderr, "I18N CANARY FAILED\n");
            std::abort();
        }
        qInfo() << "I18N canary OK:" << key << "->" << out;
    }
#endif
}

// Load a short media file and assert QMediaPlayer reaches LoadedMedia.
// Exit code 0 on success, 1 on failure. Used by CI / release smoke (P8.7).
static int runSelfTest(const QString &mediaPath)
{
    if (mediaPath.isEmpty() || !QFileInfo::exists(mediaPath)) {
        qCritical() << "--selftest: media file not found:" << mediaPath;
        return 1;
    }

    QMediaPlayer player;
    QAudioOutput audio;
    audio.setVolume(0.0);
    player.setAudioOutput(&audio);

    QEventLoop loop;
    int result = 1;
    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged,
                     &loop, [&](QMediaPlayer::MediaStatus st) {
        if (st == QMediaPlayer::LoadedMedia || st == QMediaPlayer::BufferedMedia) {
            result = 0;
            loop.quit();
        } else if (st == QMediaPlayer::InvalidMedia) {
            result = 1;
            loop.quit();
        }
    });
    QObject::connect(&player, &QMediaPlayer::errorOccurred,
                     &loop, [&](QMediaPlayer::Error, const QString &err) {
        qCritical() << "--selftest error:" << err;
        result = 1;
        loop.quit();
    });

    QTimer::singleShot(8000, &loop, &QEventLoop::quit); // hard timeout
    player.setSource(QUrl::fromLocalFile(mediaPath));

    loop.exec();

    if (result == 0)
        qInfo() << "--selftest PASSED:" << mediaPath;
    else
        qCritical() << "--selftest FAILED (status="
                    << static_cast<int>(player.mediaStatus()) << ")";
    return result;
}

int main(int argc, char *argv[])
{
    // Startup timer starts as early as possible (P0.b / gate G0–G6).
    QElapsedTimer startupTimer;
    startupTimer.start();

    int exitCode;
    do {
        QApplication app(argc, argv);

        // ⚠ DO NOT rename applicationName / organizationName.
        // They control BOTH:
        //   %LOCALAPPDATA%\VinilPlayer\Vinil Player\vinil.db
        //   HKCU\Software\VinilPlayer\Vinil Player
        // Renaming them orphans every existing user's library and settings
        // in a single commit. Legacy names are intentional (ContextProject §7.2).
        app.setApplicationName("Vinil Player");
        app.setOrganizationName("VinilPlayer");
        app.setApplicationDisplayName("Lumen Music");
        app.setApplicationVersion(QStringLiteral("2.0.0"));
        app.setWindowIcon(QIcon(":/icon.png"));

        QCommandLineParser parser;
        parser.setApplicationDescription(QStringLiteral("Lumen Music"));
        parser.addHelpOption();
        parser.addVersionOption();

        QCommandLineOption seedOpt(
            QStringLiteral("seed-fake-library"),
            QStringLiteral("Insert N synthetic tracks for performance testing."),
            QStringLiteral("N"));
        QCommandLineOption selfTestOpt(
            QStringLiteral("selftest"),
            QStringLiteral("Load MEDIA, assert LoadedMedia, exit 0/1."),
            QStringLiteral("MEDIA"));
        QCommandLineOption canaryOpt(
            QStringLiteral("i18n-canary"),
            QStringLiteral("Force the EN translation canary and exit 0/1."));
        parser.addOption(seedOpt);
        parser.addOption(selfTestOpt);
        parser.addOption(canaryOpt);
        parser.process(app);

        QSettings settings;
        Theme::setActiveTheme(Theme::themeById(settings.value("theme", "lumen").toString()));
        Lang::setActiveLang(settings.value("language", "pt").toString());

        // Optional force-EN for the canary path used by CI.
        if (parser.isSet(canaryOpt)) {
            Lang::setActiveLang(QStringLiteral("en"));
            const QString key = QStringLiteral("Músicas");
            const QString out = Lang::tr(key);
            if (out == key) {
                qCritical() << "I18N CANARY FAILED";
                return 1;
            }
            qInfo() << "I18N canary OK:" << key << "->" << out;
            return 0;
        }

        assertI18nCanary();

        if (parser.isSet(selfTestOpt)) {
            return runSelfTest(parser.value(selfTestOpt));
        }

        if (parser.isSet(seedOpt)) {
            const int n = parser.value(seedOpt).toInt();
            if (!Database::instance().open()) {
                qCritical() << "Failed to open database for seeding:"
                            << Database::instance().lastError();
                return 1;
            }
            const int inserted = Database::instance().seedFakeLibrary(n);
            qInfo() << "Seeded" << inserted << "tracks; continuing to UI.";
        }

        app.setStyleSheet(Theme::globalStyleSheet());

        QFont defaultFont("Segoe UI", 12);
        defaultFont.setWeight(QFont::Medium);
        app.setFont(defaultFont);

        MainWindow window;
        QObject::connect(&window, &MainWindow::themeChangeRequested,
                         []() { qApp->exit(RESTART_CODE); });

        // Cold-start instrumentation: main() → first paint (P0.b / gate G6).
        auto *probe = new StartupProbe(startupTimer, &window);
        window.installEventFilter(probe);
        window.show();

        exitCode = app.exec();
    } while (exitCode == RESTART_CODE);

    return exitCode;
}
