#pragma once

// quickSettingIds.h
// Names for the settings the VPN page's quick settings can show, as saved in
// app.json (AppConfig::quickSettings()). Never rename one: anyone who chose
// that setting would lose it from their quick settings.

#include <QString>

namespace QuickSettingIds
{
// App tab
inline const QString LAUNCH_ON_STARTUP        = QStringLiteral("launch-on-startup");
inline const QString AUTO_CONNECT             = QStringLiteral("auto-connect");
inline const QString AUTO_CONNECT_SERVER      = QStringLiteral("auto-connect-server");
inline const QString START_HIDDEN             = QStringLiteral("start-hidden");
inline const QString CLOSE_TO_TRAY            = QStringLiteral("close-to-tray");
inline const QString NOTIFICATIONS            = QStringLiteral("notifications");
inline const QString CHECK_FOR_UPDATES        = QStringLiteral("check-for-updates");
inline const QString RECENT_CONNECTIONS       = QStringLiteral("recent-connections");
inline const QString CLEAR_RECENT_CONNECTIONS = QStringLiteral("clear-recent-connections");
inline const QString ENABLE_FAVORITES         = QStringLiteral("enable-favorites");
inline const QString CLEAR_FAVORITES          = QStringLiteral("clear-favorites");
inline const QString LOG_TO_FILE              = QStringLiteral("log-to-file");

// Appearance tab
inline const QString THEME                    = QStringLiteral("theme");
inline const QString GLOBE_ANIMATION          = QStringLiteral("globe-animation");
inline const QString GLOBE_PAUSE              = QStringLiteral("globe-pause-when-unfocused");
inline const QString SHOW_COUNTRIES           = QStringLiteral("show-countries");
inline const QString SHOW_QUICK_SETTINGS      = QStringLiteral("show-quick-settings");
inline const QString SHOW_LOCATION_PICKER     = QStringLiteral("show-location-picker");
inline const QString SHOW_FAVORITES_DROPDOWN  = QStringLiteral("show-favorites-dropdown");

// VPN tab
inline const QString CRASH_REPORTS            = QStringLiteral("crash-reports");
inline const QString IPV6                     = QStringLiteral("ipv6");
inline const QString KILL_SWITCH              = QStringLiteral("kill-switch");
inline const QString NAT_TYPE                 = QStringLiteral("nat-type");
inline const QString VPN_ACCELERATOR          = QStringLiteral("vpn-accelerator");
inline const QString NETSHIELD                = QStringLiteral("netshield");
inline const QString PORT_FORWARDING          = QStringLiteral("port-forwarding");
inline const QString CUSTOM_DNS               = QStringLiteral("custom-dns");
} // namespace QuickSettingIds
