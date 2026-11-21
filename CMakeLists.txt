cmake_minimum_required(VERSION 3.16)
project(WHATSAPP_REBASE VERSION 1.1.0 LANGUAGES CXX)

set(APP_NAME "WhatsApp Desktop")
set(APP_VERSION "${PROJECT_VERSION}")
set(APP_DESCRIPTION "Ultra-fast, secure WhatsApp Desktop client with system tray, calling, and native media integration")
set(APP_AUTHOR "WhatsApp Desktop Team")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Default to RelWithDebInfo if not set (for GDB and perf)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# Security & Memory Safety hardening flags
add_compile_options(
    -g
    -Wall
    -Wextra
    -Wformat=2
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
    -D_GLIBCXX_ASSERTIONS
)
add_link_options(
    -Wl,-z,relro
    -Wl,-z,now
)

# Portable Qt prefix path auto-detection:
# 1. Respect explicit CMAKE_PREFIX_PATH (e.g. -DCMAKE_PREFIX_PATH=...)
# 2. Check QT_DIR environment variable
# 3. Check current user's $HOME/Qt/6.*/gcc_64 (installed via official Qt Installer)
# 4. Fall back to standard distro system packages (/usr)
if(NOT DEFINED CMAKE_PREFIX_PATH AND NOT DEFINED ENV{QT_DIR})
    file(GLOB LOCAL_USER_QT "$ENV{HOME}/Qt/6.*/gcc_64")
    if(LOCAL_USER_QT)
        list(SORT LOCAL_USER_QT ORDER DESCENDING)
        list(GET LOCAL_USER_QT 0 DETECTED_QT)
        message(STATUS "Auto-detected local user Qt installation: ${DETECTED_QT}")
        set(CMAKE_PREFIX_PATH "${DETECTED_QT}")
    endif()
elseif(DEFINED ENV{QT_DIR})
    set(CMAKE_PREFIX_PATH "$ENV{QT_DIR};${CMAKE_PREFIX_PATH}")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets PrintSupport WebEngineWidgets DBus Network Test)

set(HAVE_WEBENGINE ON)
set(HAVE_DBUS ON)
set(QT_LIBRARIES Qt6::Core Qt6::Widgets Qt6::PrintSupport Qt6::WebEngineWidgets Qt6::DBus Qt6::Network)

qt_add_resources(RESOURCES resources/resources.qrc)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    webpage.cpp
    webpage.h
    mediaschemehandler.cpp
    mediaschemehandler.h
    screensharedialog.cpp
    screensharedialog.h
    downloadmanager.cpp
    downloadmanager.h
    settingsdialog.cpp
    settingsdialog.h
    mprisplayer.cpp
    mprisplayer.h
    mediagallery.cpp
    mediagallery.h
    traymanager.cpp
    traymanager.h
    exportdialog.cpp
    exportdialog.h
    systemintegration.cpp
    systemintegration.h
    popoutwindow.cpp
    popoutwindow.h
    resources/resources.qrc
)

qt_add_executable(WHATSAPP_REBASE
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

target_link_libraries(WHATSAPP_REBASE PRIVATE ${QT_LIBRARIES})

target_compile_definitions(WHATSAPP_REBASE PRIVATE
    HAVE_WEBENGINE
    HAVE_DBUS
)

target_sources(WHATSAPP_REBASE PRIVATE ${RESOURCES})

set_target_properties(WHATSAPP_REBASE PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)

install(TARGETS WHATSAPP_REBASE
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES com.github.circuito.whatsapp.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

install(FILES resources/icons/whatsapp.png
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
    RENAME com.github.circuito.whatsapp.png
)

install(FILES com.github.circuito.whatsapp.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
)

install(FILES README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

qt_finalize_executable(WHATSAPP_REBASE)

# Automated Test Target
enable_testing()
add_executable(test_whatsapp
    tests/test_whatsapp.cpp
    webpage.cpp
    webpage.h
    mediaschemehandler.cpp
    mediaschemehandler.h
    screensharedialog.cpp
    screensharedialog.h
    downloadmanager.cpp
    downloadmanager.h
    settingsdialog.cpp
    settingsdialog.h
    mprisplayer.cpp
    mprisplayer.h
    traymanager.cpp
    traymanager.h
    systemintegration.cpp
    systemintegration.h
    popoutwindow.cpp
    popoutwindow.h
    resources/resources.qrc
)

target_link_libraries(test_whatsapp PRIVATE ${QT_LIBRARIES} Qt6::Test)
target_include_directories(test_whatsapp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(test_whatsapp PRIVATE HAVE_WEBENGINE HAVE_DBUS)
target_sources(test_whatsapp PRIVATE ${RESOURCES})
add_test(NAME test_whatsapp COMMAND test_whatsapp)
qt_finalize_executable(test_whatsapp)
