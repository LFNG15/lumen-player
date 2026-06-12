cmake_minimum_required(VERSION 3.21)

project(LumenMusic
    VERSION 2.0.0
    DESCRIPTION "Lumen Music"
    LANGUAGES CXX)

# --- C++ standard -----------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json so clangd/IntelliSense pick up the real flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Qt ---------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia Sql Network)

# Runs moc/uic automatically (equivalent to qmake's built-in handling).
qt_standard_project_setup()

# Compile .qrc resources (e.g. the app icon). qt_standard_project_setup enables
# AUTOMOC/AUTOUIC but not AUTORCC, so turn it on explicitly — otherwise the .qrc
# passed to qt_add_executable is silently ignored and ":/..." paths resolve to
# nothing at runtime.
set(CMAKE_AUTORCC ON)

# --- Target -----------------------------------------------------------------
set(PROJECT_SOURCES
    src/main.cpp
    src/database/database.cpp
    src/mainwindow.cpp
    src/pages/homepage.cpp
    src/pages/addmusicpage.cpp
    src/pages/folderspage.cpp
    src/pages/folderdetailpage.cpp
    src/pages/likedpage.cpp
    src/pages/queuepage.cpp
    src/pages/searchpage.cpp
    src/pages/importplaylistdialog.cpp
    src/player/playerbar.cpp
    src/player/trackmodel.cpp
    src/widgets/vinylwidget.cpp
    src/widgets/clickableslider.cpp
)

set(PROJECT_HEADERS
    src/mainwindow.h
    src/database/database.h
    src/pages/homepage.h
    src/pages/addmusicpage.h
    src/pages/folderspage.h
    src/pages/folderdetailpage.h
    src/pages/likedpage.h
    src/pages/queuepage.h
    src/pages/searchpage.h
    src/pages/importplaylistdialog.h
    src/player/playerbar.h
    src/player/trackmodel.h
    src/widgets/vinylwidget.h
    src/widgets/clickableslider.h
    src/widgets/theme.h
    src/widgets/hoverplayfilter.h
    src/widgets/reorderablelist.h
    src/widgets/mediatools.h
    src/widgets/lang.h
    src/widgets/coverwidget.h
    src/widgets/textutils.h
)

qt_add_executable(LumenMusic
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
    resources/resources.qrc
)

target_include_directories(LumenMusic PRIVATE
    src
    src/pages
    src/player
    src/widgets
    src/database
)

target_link_libraries(LumenMusic PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Sql
    Qt6::Network
)

# --- Windows-specific: icon, version metadata, GUI subsystem ----------------
if(WIN32)
    enable_language(RC)

    # Compile the Windows resource (app icon) in its own object library so it
    # does NOT inherit the target's include directories. The source tree lives
    # under a path with spaces and a non-ASCII char ("Área de Trabalho"), and
    # windres' internal preprocessor mangles such -I paths. With no include
    # dirs, windres resolves the relative "lumen.ico" against the .rc's own
    # directory, which works. (This is why the old .pro used a relative RC_FILE.)
    # Generate the version resource from the single source of truth (PROJECT_VERSION).
    # VERSIONINFO references no external files, so the spaced/non-ASCII path is a
    # non-issue here; it lands in the (ASCII) build dir.
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/version.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
        @ONLY)

    add_library(lumen_winres OBJECT
        resources/app.rc
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc")
    # No moc/uic/rcc here: those inject an autogen include dir (a spaced path)
    # that windres' preprocessor cannot parse.
    set_target_properties(lumen_winres PROPERTIES
        AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    target_link_libraries(LumenMusic PRIVATE lumen_winres)

    set_target_properties(LumenMusic PROPERTIES
        WIN32_EXECUTABLE ON          # GUI app: no console window
        OUTPUT_NAME LumenMusic
    )
endif()

# --- Bundle the yt-dlp helper next to the executable ------------------------
# qmake copied src/thirdparty/yt-dlp.exe into the build dir; do the same so the
# app can locate it at runtime.
set(YTDLP_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/thirdparty/yt-dlp.exe")
if(EXISTS "${YTDLP_SRC}")
    add_custom_command(TARGET LumenMusic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${YTDLP_SRC}" "$<TARGET_FILE_DIR:LumenMusic>/yt-dlp.exe"
        COMMENT "Copying yt-dlp.exe next to LumenMusic")
endif()

# --- Install + self-contained deployment ------------------------------------
# Flat layout for a portable Windows app: LumenMusic.exe + Qt DLLs at the top,
# plugins in subfolders. yt-dlp sits next to the exe (where the app looks).
install(TARGETS LumenMusic RUNTIME DESTINATION ".")
if(EXISTS "${YTDLP_SRC}")
    install(FILES "${YTDLP_SRC}" DESTINATION ".")
endif()

# Run windeployqt at install time so `cmake --install` / `cpack` produce a tree
# that runs on a machine without Qt. Deploys flat next to the exe (the layout
# the Inno Setup installer expects) instead of Qt's structured bin/ layout.
if(WIN32)
    get_filename_component(_qt_root "${Qt6_DIR}/../../.." ABSOLUTE)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS "${_qt_root}/bin")
    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR "windeployqt not found near ${_qt_root}/bin")
    endif()
    install(CODE "
        message(STATUS \"Deploying Qt runtime with windeployqt...\")
        execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
            --release --compiler-runtime --no-translations --no-system-d3d-compiler
            \"\${CMAKE_INSTALL_PREFIX}/LumenMusic.exe\"
            RESULT_VARIABLE _rc)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR \"windeployqt failed (\${_rc})\")
        endif()
    ")
endif()

# --- Packaging (CPack) ------------------------------------------------------
# `cpack -G ZIP` builds the deployed install tree and zips it into a release
# artifact named after the single-source version.
set(CPACK_PACKAGE_NAME "LumenMusic")
set(CPACK_PACKAGE_VENDOR "Lumen Connection")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "LumenMusic-v${PROJECT_VERSION}-win64")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Lumen Music")
set(CPACK_GENERATOR "ZIP")
set(CPACK_STRIP_FILES ON)
include(CPack)
