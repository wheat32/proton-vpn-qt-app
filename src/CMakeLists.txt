# 3.21 covers every current distro (Ubuntu 22.04 ships 3.22, Debian 12 ships
# 3.25). Nothing in this project needs anything newer, and requiring a version
# newer than the distro's blocks source builds for no benefit.
cmake_minimum_required(VERSION 3.21)
project(proton_vpn_qt)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Strict conformance, no GNU/MSVC extensions
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS
        Core
        Gui
        Network
        Widgets
        Svg
        SvgWidgets
        DBus
        REQUIRED)

find_package(Qt6 COMPONENTS LinguistTools QUIET)

set(SOURCES
        main.cpp
        appConfig.h
        appConfig.cpp
        fileLogger.h
        fileLogger.cpp
        migrations.h
        migrations.cpp
        themeManager.h
        themeManager.cpp
        connectionHistory.h
        connectionHistory.cpp
        favoritesManager.h
        favoritesManager.cpp
        geoUtils.h
        geoUtils.cpp
        globe/globeMath.h
        globe/globeRenderer.h
        globe/globeRenderer.cpp
        globe/countryCenters.h
        globe/countryCenters.cpp
        motionPreference.h
        motionPreference.cpp
        jsonFile.h
        uiHelpers.h
        vpnManager.h
        vpnManager.cpp
        dbus/vpnStatusAdaptor.h
        dbus/vpnStatusAdaptor.cpp
        cli/protonvpnCli.cpp
        cli/natpmpManager.h
        cli/natpmpManager.cpp
        cli/statusMonitor.h
        cli/statusMonitor.cpp
        cli/appImageUtils.h
        cli/cliNoiseFilter.h
        cli/cliSession.h
        cli/cliSettings.h
        cli/cliTable.h
        cli/cliVersion.h
        cli/logRedaction.h
        cli/signin/signinFlow.h
        cli/signin/signinFlow.cpp
        cli/signin/legacySigninFlow.h
        cli/signin/securityKeySigninFlow.h
        cli/signin/securityKeySigninFlow.cpp
        cli/signin/signinFlows.h
        cli/signin/signinFlows.cpp
        cli/flatpakUtils.h
        cli/platformUtils.h
        mainWindow.h
        mainWindow.cpp
        widgets/elidaLabel.h
        widgets/stackPage.h
        widgets/infoBanner.h
        widgets/numberSpinner.h
        widgets/globeWidget.h
        widgets/pixmapCache.h
        widgets/globeWidget.cpp
        widgets/gripSplitter.h
        widgets/pickerBase.h
        widgets/pickerDrawer.h
        widgets/pickerDrawer.cpp
        widgets/styleUtils.h
        widgets/svgBanner.h
        widgets/toastNotification.h
        widgets/toggleSwitch.h
        widgets/toggleSwitch.cpp
        widgets/toggleWithStatus.h
        pages/notInstalledPage.h
        pages/notInstalledPage.cpp
        pages/cliNotRespondingPage.h
        pages/cliNotRespondingPage.cpp
        pages/loginPage.h
        pages/loginPage.cpp
        pages/vpnPage.h
        pages/vpnPage.cpp
        pages/countriesPage.h
        pages/countriesPage.cpp
        pages/accountPage.h
        pages/accountPage.cpp
        pages/settingsPage.h
        pages/settingsPage.cpp
        pages/debugPage.h
        pages/debugPage.cpp
        dialogs/aboutDialog.h
        dialogs/aboutDialog.cpp
        dialogs/errorDetailsDialog.h
        dialogs/errorDetailsDialog.cpp
        dialogs/quitDialog.h
        dialogs/quitDialog.cpp
        dialogs/updateAvailableDialog.h
        dialogs/updateAvailableDialog.cpp
        dialogs/whatsNewDialog.h
        dialogs/whatsNewDialog.cpp
        resources.qrc
        flags.qrc
)

add_executable(proton_vpn_qt ${SOURCES})

# The tree is warning-clean under these; keeping them on means it stays that way.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(proton_vpn_qt PRIVATE -Wall -Wextra)
endif()

if(Qt6LinguistTools_FOUND)
    qt_add_translations(proton_vpn_qt
            TS_FILES
                i18n/proton_vpn_qt_en.ts
            RESOURCE_PREFIX "/i18n"
            NO_GENERATE_PLURALS_TS_FILE
    )
else()
    message(WARNING "Qt6LinguistTools not found - translations will not be compiled.")
endif()

target_link_libraries(proton_vpn_qt
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Widgets
        Qt6::Svg
        Qt6::SvgWidgets
        Qt6::DBus
)

install(TARGETS proton_vpn_qt
        RUNTIME DESTINATION bin
)

# The desktop file and icon are installed under the app ID, matching the
# metainfo's <launchable> and the names Flatpak requires.
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../proton-vpn-qt-app.desktop
        DESTINATION share/applications
        RENAME io.github.wheat32.ProtonVPNQt.desktop
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../proton-vpn-sign.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME io.github.wheat32.ProtonVPNQt.svg
)

# The metainfo is generated so its release always matches version.json. The
# release date is the date of the commit being built: releases are tagged on the
# commit pushed to main, so for a release it's the day it was published. Without
# git (e.g. a source archive), it falls back to the build date.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/version.json VERSION_JSON)
string(JSON APP_VERSION GET "${VERSION_JSON}" app_version)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS version.json)

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --format=%cd --date=short
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE APP_RELEASE_DATE
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )
endif()
if("${APP_RELEASE_DATE}" STREQUAL "")
    string(TIMESTAMP APP_RELEASE_DATE "%Y-%m-%d" UTC)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../io.github.wheat32.ProtonVPNQt.metainfo.xml.in
        ${CMAKE_CURRENT_BINARY_DIR}/io.github.wheat32.ProtonVPNQt.metainfo.xml
        @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.wheat32.ProtonVPNQt.metainfo.xml
        DESTINATION share/metainfo
)

#  Tests
enable_testing()
add_subdirectory(tests)



