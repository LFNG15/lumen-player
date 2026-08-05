cmake_minimum_required(VERSION 3.21)

project(LumenMusic
    VERSION 2.0.0
    DESCRIPTION "Lumen Music"
    LANGUAGES CXX)

# --- C++ standard -----------------------------------------------------------
# P0.a′: C++20 (separate from the MinGW→MSVC switch for bisect). Required by
# C++/WinRT (P5) and enables designated initializers for design tokens (P1).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json so clangd/IntelliSense pick up the real flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- MSVC toolchain flags (P0.a: MinGW → MSVC) ------------------------------
# /utf-8 is mandatory: sources are UTF-8 without BOM and non-ASCII string
# literals are Lang::tr() hash keys. Without it MSVC reads them as CP1252,
# English translations die silently, and the UI becomes mojibake.
if(MSVC)
    add_compile_options(/utf-8 /Zc:__cplusplus /permissive- /EHsc /W3 /MP)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")  # /MD, matches official Qt builds
endif()

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
    src/database/migrator.cpp
    src/design/tokens.cpp
    src/design/palettes.cpp
    src/design/thememanager.cpp
    src/design/stylesheet.cpp
    src/design/components.cpp
    src/design/paint.cpp
    src/design/i18n.cpp
    src/mainwindow.cpp
    src/pages/homepage.cpp
    src/pages/addmusicpage.cpp
    src/pages/folderspage.cpp
    src/pages/folderdetailpage.cpp
    src/pages/likedpage.cpp
    src/pages/queuepage.cpp
    src/pages/searchpage.cpp
    src/pages/librarypage.cpp
    src/pages/importplaylistdialog.cpp
    src/player/playerbar.cpp
    src/player/playbackengine.cpp
    src/player/trackmodel.cpp
    src/models/tracklistmodel.cpp
    src/models/trackfilterproxy.cpp
    src/models/trackrowdelegate.cpp
    src/models/trackcontextmenu.cpp
    src/platform/mediakeys.cpp
    src/platform/trayicon.cpp
    src/tools/ytdlp_bootstrap.cpp
    src/widgets/vinylwidget.cpp
    src/widgets/clickableslider.cpp
)

set(PROJECT_HEADERS
    src/mainwindow.h
    src/database/database.h
    src/database/migrator.h
    src/design/tokens.h
    src/design/palettes.h
    src/design/thememanager.h
    src/design/stylesheet.h
    src/design/components.h
    src/design/paint.h
    src/design/icons.h
    src/design/i18n.h
    src/pages/homepage.h
    src/pages/addmusicpage.h
    src/pages/folderspage.h
    src/pages/folderdetailpage.h
    src/pages/likedpage.h
    src/pages/queuepage.h
    src/pages/searchpage.h
    src/pages/librarypage.h
    src/pages/importplaylistdialog.h
    src/player/playerbar.h
    src/player/playbackengine.h
    src/player/trackmodel.h
    src/models/tracklistmodel.h
    src/models/trackfilterproxy.h
    src/models/trackrowdelegate.h
    src/models/trackcontextmenu.h
    src/platform/nowplaying.h
    src/platform/mediakeys.h
    src/platform/trayicon.h
    src/tools/ytdlp_bootstrap.h
    src/database/position_gap.h
    src/database/owner_decision.h
    src/widgets/vinylwidget.h
    src/widgets/clickableslider.h
    src/widgets/theme.h
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
    src/design
    src/models
    src/platform
    src/tools
)

target_link_libraries(LumenMusic PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Sql
    Qt6::Network
)

# --- SMTC (SystemMediaTransportControls) via C++/WinRT — MSVC only ----------
# No NuGet / no cppwinrt.exe: headers ship with the Windows SDK (verified).
# Exactly one of nowplaying_win.cpp / nowplaying_null.cpp is compiled.
if(WIN32 AND MSVC)
    option(LUMEN_WITH_SMTC "SystemMediaTransportControls integration" ON)
    include(CheckIncludeFileCXX)
    # Need the MSVC + /utf-8 flags already applied for this check.
    check_include_file_cxx("winrt/Windows.Media.h" HAVE_CPPWINRT)
    if(LUMEN_WITH_SMTC AND HAVE_CPPWINRT)
        target_sources(LumenMusic PRIVATE src/platform/nowplaying_win.cpp)
        target_link_libraries(LumenMusic PRIVATE WindowsApp.lib)
        target_compile_definitions(LumenMusic PRIVATE LUMEN_HAS_SMTC)
        message(STATUS "SMTC: enabled (C++/WinRT found)")
    else()
        target_sources(LumenMusic PRIVATE src/platform/nowplaying_null.cpp)
        message(STATUS "SMTC: disabled (LUMEN_WITH_SMTC=${LUMEN_WITH_SMTC}, HAVE_CPPWINRT=${HAVE_CPPWINRT})")
    endif()
else()
    target_sources(LumenMusic PRIVATE src/platform/nowplaying_null.cpp)
endif()

# --- Windows-specific: icon, version metadata, GUI subsystem ----------------
if(WIN32)
    enable_language(RC)

    # Generate VERSIONINFO from the single source of truth (PROJECT_VERSION).
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/version.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
        @ONLY)

    # Attach .rc files directly to the exe. The old OBJECT-library workaround
    # (lumen_winres) existed only for MinGW windres, which mangled -I paths
    # containing spaces/accents. The tree now lives under an ASCII path and the
    # toolchain is MSVC (rc.exe), so the isolation layer is no longer needed.
    target_sources(LumenMusic PRIVATE
        resources/app.rc
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc")

    set_target_properties(LumenMusic PROPERTIES
        WIN32_EXECUTABLE ON          # GUI app: no console window
        OUTPUT_NAME LumenMusic
    )
endif()

# yt-dlp is NO LONGER bundled (P8.4). First-run download lives in
# src/tools/ytdlp_bootstrap.cpp → %LOCALAPPDATA%/…/tools/yt-dlp.exe

# --- Install + self-contained deployment ------------------------------------
# Flat layout for the Inno Setup tree (installer/lumen-music.iss):
#   dist/LumenMusic/LumenMusic.exe + Qt DLLs + plugins/
#
# Config-aware windeployqt: never hardcode --release (a Debug install would
# otherwise ship the wrong Qt DLLs — Task.md P7). Paths are resolved to
# absolute inside install(CODE) so relative --prefix still works.
set(CMAKE_INSTALL_BINDIR ".")

install(TARGETS LumenMusic
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION ${CMAKE_INSTALL_BINDIR})

if(WIN32)
    # MSVC: ship the VC++ redistributable DLLs (--compiler-runtime is MinGW-only).
    # Flat next to the exe (not bin/), matching the Inno tree.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
    include(InstallRequiredSystemLibraries)

    get_filename_component(_qt_root "${Qt6_DIR}/../../.." ABSOLUTE)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS "${_qt_root}/bin")
    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR "windeployqt not found near ${_qt_root}/bin")
    endif()

    # Also expose a post-build convenience for local dev (does not replace install).
    if(COMMAND qt_generate_deploy_app_script)
        message(STATUS "Deploy: windeployqt (flat Inno layout); qt_generate_deploy_app_script available but skipped for flat prefix")
    endif()

    install(CODE "
        # Resolve prefix to an absolute path — required by windeployqt/qt.conf.
        get_filename_component(_prefix \"\${CMAKE_INSTALL_PREFIX}\" ABSOLUTE)
        set(_exe \"\${_prefix}/LumenMusic.exe\")
        if(NOT EXISTS \"\${_exe}\")
            message(FATAL_ERROR \"Deploy target missing: \${_exe}\")
        endif()

        set(_deploy_flag --release)
        if(CMAKE_INSTALL_CONFIG_NAME STREQUAL \"Debug\")
            set(_deploy_flag --debug)
        endif()

        message(STATUS \"Deploying Qt runtime (\${_deploy_flag}) to \${_prefix}...\")
        execute_process(
            COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                \${_deploy_flag}
                --no-translations
                --no-system-d3d-compiler
                --dir \"\${_prefix}\"
                \"\${_exe}\"
            RESULT_VARIABLE _rc
            OUTPUT_VARIABLE _out
            ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR \"windeployqt failed (\${_rc}):\n\${_out}\n\${_err}\")
        endif()

        # Gate: multimedia FFmpeg backend + SQLite driver must be present.
        set(_ffmpeg \"\${_prefix}/plugins/multimedia/ffmpegmediaplugin.dll\")
        set(_ffmpeg_alt \"\${_prefix}/multimedia/ffmpegmediaplugin.dll\")
        set(_sqlite \"\${_prefix}/plugins/sqldrivers/qsqlite.dll\")
        set(_sqlite_alt \"\${_prefix}/sqldrivers/qsqlite.dll\")
        if(NOT EXISTS \"\${_ffmpeg}\" AND NOT EXISTS \"\${_ffmpeg_alt}\")
            message(FATAL_ERROR \"Deploy missing ffmpegmediaplugin.dll under \${_prefix}\")
        endif()
        if(NOT EXISTS \"\${_sqlite}\" AND NOT EXISTS \"\${_sqlite_alt}\")
            message(FATAL_ERROR \"Deploy missing qsqlite.dll under \${_prefix}\")
        endif()
        message(STATUS \"Deploy OK: ffmpeg + sqlite plugins present\")
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
# CPACK_STRIP_FILES is a GCC/binutils concept; leave it off under MSVC.
set(CPACK_STRIP_FILES OFF)
if(MSVC)
    # PDB for post-mortem; /OPT:REF /OPT:ICF keep the Release binary lean.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi /O2")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
endif()
include(CPack)

# --- Tests (P8.5) ------------------------------------------------------------
option(LUMEN_BUILD_TESTS "Build QTest unit tests" ON)
if(LUMEN_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    # ctest runs test exes without the developer shell PATH — prepend Qt bin so
    # Qt6*.dll (and platforms/) resolve. 0xc0000135 without this on MSVC.
    get_filename_component(_lumen_qt_bin "${Qt6_DIR}/../../../bin" ABSOLUTE)
    if(WIN32)
        set(_lumen_test_path "${_lumen_qt_bin};$ENV{PATH}")
        # Escape semicolons for CMake list→string ENVIRONMENT property.
        string(REPLACE ";" "\\;" _lumen_test_path_esc "${_lumen_test_path}")
    else()
        set(_lumen_test_path_esc "PATH=${_lumen_qt_bin}:$ENV{PATH}")
    endif()

    function(lumen_add_test name)
        add_executable(${name} ${ARGN})
        target_include_directories(${name} PRIVATE
            src src/database src/design src/models src/widgets src/player src/tools)
        target_link_libraries(${name} PRIVATE
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Test)
        if(MSVC)
            target_compile_options(${name} PRIVATE /utf-8)
        endif()
        add_test(NAME ${name} COMMAND ${name})
        if(WIN32)
            set_tests_properties(${name} PROPERTIES
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                ENVIRONMENT "PATH=${_lumen_test_path_esc}")
        else()
            set_tests_properties(${name} PROPERTIES
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                ENVIRONMENT "${_lumen_test_path_esc}")
        endif()
    endfunction()

    lumen_add_test(test_textutils
        tests/test_textutils.cpp)

    lumen_add_test(test_position_gap
        tests/test_position_gap.cpp)

    lumen_add_test(test_owner_decision
        tests/test_owner_decision.cpp)

    lumen_add_test(test_trackrow_hittest
        tests/test_trackrow_hittest.cpp
        src/models/trackrowdelegate.cpp
        src/design/paint.cpp
        src/design/tokens.cpp
        src/design/palettes.cpp
        src/design/thememanager.cpp
        src/design/stylesheet.cpp)
    target_link_libraries(test_trackrow_hittest PRIVATE Qt6::Network)

    lumen_add_test(test_migrator
        tests/test_migrator.cpp
        src/database/migrator.cpp)

    lumen_add_test(test_database_playlist
        tests/test_database_playlist.cpp
        src/database/database.cpp
        src/database/migrator.cpp
        src/player/trackmodel.cpp
        src/design/tokens.cpp
        src/design/palettes.cpp
        src/design/thememanager.cpp
        src/design/stylesheet.cpp
        src/design/paint.cpp)
    target_link_libraries(test_database_playlist PRIVATE Qt6::Network)
endif()
