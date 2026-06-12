QT       += core gui widgets multimedia sql network

TARGET = LumenMusic
TEMPLATE = app

CONFIG += c++17

# Windows executable icon + metadata.
# Use a custom .rc with a relative icon path: windres (MinGW) can't open the
# absolute path because it contains a non-ASCII character ("Área").
win32 {
    RC_FILE = resources/app.rc
    VERSION = 1.1.0
    QMAKE_TARGET_PRODUCT = Lumen Music
    QMAKE_TARGET_DESCRIPTION = Lumen Music
    QMAKE_TARGET_COMPANY = Lumen Connection

    HELPER_SRC = $$PWD/src/thirdparty/yt-dlp.exe
    HELPER_DST = $$OUT_PWD/yt-dlp.exe

    copyhelper.input = HELPER_SRC
    copyhelper.output = HELPER_DST
    HELPER_SRC_REL = $$relative_path($$HELPER_SRC, $$OUT_PWD)
    HELPER_DST_REL = $$relative_path($$HELPER_DST, $$OUT_PWD)
    copyhelper.commands = $$QMAKE_COPY $$shell_quote($$shell_path($$HELPER_SRC_REL)) $$shell_quote($$shell_path($$HELPER_DST_REL))
    copyhelper.CONFIG += target_predeps no_link
    copyhelper.name = YT-DLP
    QMAKE_EXTRA_COMPILERS += copyhelper

    helper.files = $$PWD/src/thirdparty/yt-dlp.exe
    helper.path = $$INSTALL_PREFIX
    INSTALLS += helper
}

INCLUDEPATH += \
    src \
    src/pages \
    src/player \
    src/widgets \
    src/database

SOURCES += \
    src/main.cpp \
    src/database/database.cpp \
    src/mainwindow.cpp \
    src/pages/homepage.cpp \
    src/pages/addmusicpage.cpp \
    src/pages/folderspage.cpp \
    src/pages/folderdetailpage.cpp \
    src/pages/likedpage.cpp \
    src/pages/queuepage.cpp \
    src/pages/searchpage.cpp \
    src/pages/importplaylistdialog.cpp \
    src/player/playerbar.cpp \
    src/player/trackmodel.cpp \
    src/widgets/vinylwidget.cpp \
    src/widgets/clickableslider.cpp

HEADERS += \
    src/mainwindow.h \
    src/database/database.h \
    src/pages/homepage.h \
    src/pages/addmusicpage.h \
    src/pages/folderspage.h \
    src/pages/folderdetailpage.h \
    src/pages/likedpage.h \
    src/pages/queuepage.h \
    src/pages/searchpage.h \
    src/pages/importplaylistdialog.h \
    src/player/playerbar.h \
    src/player/trackmodel.h \
    src/widgets/vinylwidget.h \
    src/widgets/clickableslider.h \
    src/widgets/theme.h \
    src/widgets/hoverplayfilter.h \
    src/widgets/reorderablelist.h \
    src/widgets/mediatools.h \
    src/widgets/lang.h \
    src/widgets/coverwidget.h \
    src/widgets/textutils.h \
    src/widgets/lumenlogo.h

RESOURCES += \
    resources/resources.qrc
