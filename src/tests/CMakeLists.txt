cmake_minimum_required(VERSION 3.21)

enable_testing()

find_package(Qt6 COMPONENTS Test REQUIRED)

#  Helper macro
# add_qt_test(name sources...)
# Creates an executable, links Qt6::Test + Qt6::Core, and registers it with
# CTest so `ctest` (or cmake --build . --target test) picks it up.
macro(add_qt_test name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_link_libraries(${name}
        Qt6::Core
        Qt6::Test
    )
    add_test(NAME ${name} COMMAND ${name})
endmacro()

#  GeoUtils
add_qt_test(tst_geoUtils
    tst_geoUtils.cpp
    ../geoUtils.h
    ../geoUtils.cpp
    # Assets are compiled in so the svgPixmap tests render a real SVG; without
    # them every pixmap would come back blank and the checks would be vacuous.
    ../resources.qrc
)
target_link_libraries(tst_geoUtils Qt6::Gui Qt6::Svg Qt6::SvgWidgets)

#  AppConfig
add_qt_test(tst_appConfig
    tst_appConfig.cpp
    ../appConfig.h
    ../appConfig.cpp
    ../fileLogger.h
    ../fileLogger.cpp
)

#  ConnectionHistory
add_qt_test(tst_connectionHistory
    tst_connectionHistory.cpp
    ../connectionHistory.h
    ../connectionHistory.cpp
    ../appConfig.h
    ../appConfig.cpp
    ../fileLogger.h
    ../fileLogger.cpp
)

#  StatusMonitor (static parsing helpers only - no subprocess spawned)
add_qt_test(tst_statusMonitor
    tst_statusMonitor.cpp
    ../cli/statusMonitor.h
    ../cli/statusMonitor.cpp
    ../cli/cliNoiseFilter.h
    ../cli/flatpakUtils.h
    ../fileLogger.h
    ../fileLogger.cpp
)

#  FavoritesManager
add_qt_test(tst_favoritesManager
    tst_favoritesManager.cpp
    ../favoritesManager.h
    ../favoritesManager.cpp
    ../fileLogger.h
    ../fileLogger.cpp
)

#  CliNoiseFilter (header-only)
add_qt_test(tst_cliNoiseFilter
    tst_cliNoiseFilter.cpp
    ../cli/cliNoiseFilter.h
)

#  Globe (math, renderer, country centers)
add_qt_test(tst_globe
    tst_globe.cpp
    ../globe/globeMath.h
    ../globe/globeRenderer.h
    ../globe/globeRenderer.cpp
    ../globe/countryCenters.h
    ../globe/countryCenters.cpp
    # The country center table is a compiled-in resource.
    ../resources.qrc
)
target_link_libraries(tst_globe Qt6::Gui)

#  MotionPreference
add_qt_test(tst_motionPreference
    tst_motionPreference.cpp
    ../motionPreference.h
    ../motionPreference.cpp
    ../appConfig.h
    ../appConfig.cpp
    ../fileLogger.h
    ../fileLogger.cpp
)
target_link_libraries(tst_motionPreference Qt6::DBus)

#  CliSession (header-only)
add_qt_test(tst_cliSession
    tst_cliSession.cpp
    ../cli/cliSession.h
)

#  CliSettings (header-only)
add_qt_test(tst_cliSettings
    tst_cliSettings.cpp
    ../cli/cliSettings.h
    ../cli/cliTable.h
)

#  ServerList (header-only)
add_qt_test(tst_serverList
    tst_serverList.cpp
    ../cli/serverList.h
)

#  CliTable (header-only)
add_qt_test(tst_cliTable
    tst_cliTable.cpp
    ../cli/cliTable.h
)

#  ElideLabel (header-only)
add_qt_test(tst_elideLabel
    tst_elideLabel.cpp
    ../widgets/elidaLabel.h
)
target_link_libraries(tst_elideLabel Qt6::Widgets)

#  PixmapCache (header-only)
add_qt_test(tst_pixmapCache
    tst_pixmapCache.cpp
    ../widgets/pixmapCache.h
)
target_link_libraries(tst_pixmapCache Qt6::Gui)

#  LogRedaction (header-only)
add_qt_test(tst_logRedaction
    tst_logRedaction.cpp
    ../cli/logRedaction.h
)

#  PlatformUtils (header-only)
add_qt_test(tst_platformUtils
    tst_platformUtils.cpp
    ../cli/platformUtils.h
    ../cli/appImageUtils.h
    ../cli/flatpakUtils.h
)

#  UiHelpers
add_qt_test(tst_uiHelpers
    tst_uiHelpers.cpp
    ../uiHelpers.h
)

#  FlatpakUtils
add_qt_test(tst_flatpakUtils
    tst_flatpakUtils.cpp
    ../cli/flatpakUtils.h
)

#  CliVersion (header-only)
add_qt_test(tst_cliVersion
    tst_cliVersion.cpp
    cliBanner.h
    ../cli/cliVersion.h
)

#  Sign-in: choosing the flow for the CLI version
add_qt_test(tst_signinFlows
    tst_signinFlows.cpp
    cliBanner.h
    signinTestUtils.h
    ../cli/cliVersion.h
    ../cli/signin/signinFlow.h
    ../cli/signin/signinFlow.cpp
    ../cli/signin/legacySigninFlow.h
    ../cli/signin/securityKeySigninFlow.h
    ../cli/signin/securityKeySigninFlow.cpp
    ../cli/signin/signinFlows.h
    ../cli/signin/signinFlows.cpp
)

#  Sign-in with CLI 1.0.0 to 1.0.3
add_qt_test(tst_legacySigninFlow
    tst_legacySigninFlow.cpp
    signinTestUtils.h
    ../cli/signin/signinFlow.h
    ../cli/signin/signinFlow.cpp
    ../cli/signin/legacySigninFlow.h
)

#  Sign-in with CLI 1.0.4 and newer (security keys, --totp)
add_qt_test(tst_securityKeySigninFlow
    tst_securityKeySigninFlow.cpp
    signinTestUtils.h
    ../cli/signin/signinFlow.h
    ../cli/signin/signinFlow.cpp
    ../cli/signin/securityKeySigninFlow.h
    ../cli/signin/securityKeySigninFlow.cpp
)

#  StackPage (header-only)
add_qt_test(tst_stackPage
    tst_stackPage.cpp
    ../widgets/stackPage.h
)
target_link_libraries(tst_stackPage Qt6::Widgets)

#  HintBubble placement
add_qt_test(tst_hintBubble
    tst_hintBubble.cpp
    ../widgets/hintBubble.h
    ../widgets/hintBubble.cpp
)
target_link_libraries(tst_hintBubble Qt6::Widgets)
