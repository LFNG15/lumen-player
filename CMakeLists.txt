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
    src/pages/importplaylistdialog.cpp
    src/player/playerbar.cpp
    src/player/trackmodel.cpp
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
    src/design
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

# --- Bundle the yt-dlp helper next to the executable ------------------------
# qmake copied src/thirdparty/yt-dlp.exe into the build dir; do the same so the
# app can locate it at runtime. (P8.4 will replace this with first-run download.)
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

    # MSVC: --compiler-runtime is a MinGW concept. Ship the VC++ redistributable
    # DLLs via InstallRequiredSystemLibraries instead.
    include(InstallRequiredSystemLibraries)
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION ".")
    endif()

    install(CODE "
        message(STATUS \"Deploying Qt runtime with windeployqt...\")
        set(_deploy_flag --release)
        if(CMAKE_INSTALL_CONFIG_NAME STREQUAL \"Debug\")
            set(_deploy_flag --debug)
        endif()
        execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
            \${_deploy_flag}
            --no-translations --no-system-d3d-compiler
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
# CPACK_STRIP_FILES is a GCC/binutils concept; leave it off under MSVC.
set(CPACK_STRIP_FILES OFF)
if(MSVC)
    # PDB for post-mortem; /OPT:REF /OPT:ICF keep the Release binary lean.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi /O2")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
endif()
include(CPack)
