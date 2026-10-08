#include <algorithm>
#include <QFile>
// ReSharper disable once CppUnusedIncludeDirective
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include "appConfig.h"
#include "debug.h"
#include "fileLogger.h"
#include "jsonFile.h"
#include "quickSettingIds.h"

namespace
{
QString configFile() { return AppConfig::configDir() + QStringLiteral("/app.json"); }

constexpr int DEFAULT_RECENT_CONNECTIONS_COUNT = 5;

// How the settings log writes a boolean.
QString boolText(const bool value)
{
    return value == true ? QStringLiteral("true") : QStringLiteral("false");
}

// The quick settings as they may be kept: each once, and no more than fit.
QStringList normalizedQuickSettings(QStringList ids)
{
    ids.removeDuplicates();
    return ids.mid(0, AppConfig::QUICK_SETTINGS_MAX);
}
} // namespace

// static
// Easy-to-change config location
// QStandardPaths::GenericConfigLocation resolves to:
//   - Native install : ~/.config/ProtonVPN-Qt/
//   - Flatpak sandbox: ~/.var/app/io.github.wheat32.ProtonVPNQt/config/ProtonVPN-Qt/
QString AppConfig::configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/ProtonVPN-Qt");
}

// static
QString AppConfig::globeAnimationName(const GlobeAnimation value)
{
    switch (value)
    {
        case GlobeAnimation::On:
            return QStringLiteral("on");

        case GlobeAnimation::Off:
            return QStringLiteral("off");

        default:
            return QStringLiteral("auto");
    }
}

// static
AppConfig::GlobeAnimation AppConfig::globeAnimationFromName(const QString& name)
{
    if (name == globeAnimationName(GlobeAnimation::On))
    {
        return GlobeAnimation::On;
    }
    if (name == globeAnimationName(GlobeAnimation::Off))
    {
        return GlobeAnimation::Off;
    }
    return GlobeAnimation::Auto;
}

// static
QString AppConfig::themeName(const Theme theme)
{
    switch (theme)
    {
        case Theme::Dark:
            return QStringLiteral("dark");

        case Theme::Light:
            return QStringLiteral("light");

        default:
            return QStringLiteral("system");
    }
}

AppConfig& AppConfig::instance()
{
    static AppConfig inst;
    return inst;
}

AppConfig::AppConfig()
{
    load();
}

void AppConfig::load()
{
    QFile f(configFile());
    if (f.open(QIODevice::ReadOnly) == false)
        return; // file doesn't exist yet - all values stay at defaults

    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    m_autoConnect = obj.value(QStringLiteral("auto_connect")).toBool(false);
    m_autoConnectServer = obj.value(QStringLiteral("auto_connect_server")).toString();
    m_notifications = obj.value(QStringLiteral("notifications")).toBool(true);
    // Clamped on read as well as on write: the file is user-editable, and a
    // negative count would make entries() slice with a negative length.
    m_recentConnectionsCount = qMax(0,
        obj.value(QStringLiteral("recent_connections_count")).toInt(DEFAULT_RECENT_CONNECTIONS_COUNT));
    m_startHidden = obj.value(QStringLiteral("start_hidden")).toBool(false);
    m_closeToTray = obj.value(QStringLiteral("close_to_tray")).toBool(true);
    m_showLocationPicker = obj.value(QStringLiteral("show_location_picker")).toBool(true);
    m_showFavoritesDropdown = obj.value(QStringLiteral("show_favorites_dropdown")).toBool(true);
    m_showCountriesSidebar = obj.value(QStringLiteral("show_countries_sidebar")).toBool(true);
    m_showQuickSettings = obj.value(QStringLiteral("show_quick_settings")).toBool(true);
    // Missing until the quick settings are first changed; an empty list is
    // the user's choice of none.
    if (obj.contains(QStringLiteral("quick_settings")) == true)
    {
        QStringList ids;
        for (const QJsonValue& id : obj.value(QStringLiteral("quick_settings")).toArray())
        {
            ids << id.toString();
        }
        ids.removeAll(QString());
        m_quickSettings = normalizedQuickSettings(ids);
    }
    m_favoritesEnabled = obj.value(QStringLiteral("favorites_enabled")).toBool(true);
    m_lastSeenVersion = obj.value(QStringLiteral("last_seen_version")).toString();
    m_checkForUpdates = obj.value(QStringLiteral("check_for_updates")).toBool(true);
    m_logToFile = obj.value(QStringLiteral("log_to_file")).toBool(false);
    m_verboseLogging = obj.value(QStringLiteral("verbose_logging")).toBool(false);
    VerboseLog::setSetting(m_verboseLogging);
    // Clamped on read too: the file is user-editable, and a ratio outside the
    // range would squeeze one side of the split view to nothing.
    m_splitViewCountriesRatio = std::clamp(
        obj.value(QStringLiteral("split_view_countries_ratio")).toDouble(SPLIT_RATIO_DEFAULT),
        SPLIT_RATIO_MIN, SPLIT_RATIO_MAX);

    m_globePauseWhenUnfocused = obj.value(QStringLiteral("globe_pause_when_unfocused")).toBool(false);
    m_globeAnimation = globeAnimationFromName(obj.value(QStringLiteral("globe_animation")).toString());

    const QString themeStr = obj.value(QStringLiteral("theme")).toString(QStringLiteral("system"));
    if (themeStr == QStringLiteral("dark"))
    {
        m_theme = Theme::Dark;
    }
    else if (themeStr == QStringLiteral("light"))
    {
        m_theme = Theme::Light;
    }
    else
    {
        m_theme = Theme::System;
    }
}

void AppConfig::logLoadedConfig() const
{
    DBG_SETTINGS(QStringLiteral("Config loaded from: ") + configFile());
    DBG_SETTINGS(QStringLiteral("  auto_connect             = ") + boolText(m_autoConnect));
    DBG_SETTINGS(QStringLiteral("  auto_connect_server      = ") + m_autoConnectServer);
    DBG_SETTINGS(QStringLiteral("  notifications            = ") + boolText(m_notifications));
    DBG_SETTINGS(QStringLiteral("  recent_connections_count = ") + QString::number(m_recentConnectionsCount));
    DBG_SETTINGS(QStringLiteral("  start_hidden             = ") + boolText(m_startHidden));
    DBG_SETTINGS(QStringLiteral("  close_to_tray            = ") + boolText(m_closeToTray));
    DBG_SETTINGS(QStringLiteral("  show_location_picker     = ") + boolText(m_showLocationPicker));
    DBG_SETTINGS(QStringLiteral("  show_favorites_dropdown  = ") + boolText(m_showFavoritesDropdown));
    DBG_SETTINGS(QStringLiteral("  show_countries_sidebar   = ") + boolText(m_showCountriesSidebar));
    DBG_SETTINGS(QStringLiteral("  show_quick_settings      = ") + boolText(m_showQuickSettings));
    DBG_SETTINGS(QStringLiteral("  quick_settings           = ") + m_quickSettings.join(QStringLiteral(", ")));
    DBG_SETTINGS(QStringLiteral("  favorites_enabled        = ") + boolText(m_favoritesEnabled));
    DBG_SETTINGS(QStringLiteral("  last_seen_version        = ") + m_lastSeenVersion);
    DBG_SETTINGS(QStringLiteral("  check_for_updates        = ") + boolText(m_checkForUpdates));
    DBG_SETTINGS(QStringLiteral("  log_to_file              = ") + boolText(m_logToFile));
    DBG_SETTINGS(QStringLiteral("  verbose_logging          = ") + boolText(m_verboseLogging));
    DBG_SETTINGS(QStringLiteral("  split_view_countries_ratio = ") + QString::number(m_splitViewCountriesRatio));
    DBG_SETTINGS(QStringLiteral("  theme                    = ") + themeName(m_theme));
    DBG_SETTINGS(QStringLiteral("  globe_animation          = ") + globeAnimationName(m_globeAnimation));
    DBG_SETTINGS(QStringLiteral("  globe_pause_when_unfocused = ") + boolText(m_globePauseWhenUnfocused));
}

bool AppConfig::save() const
{
    QJsonObject obj;
    obj[QStringLiteral("auto_connect")] = m_autoConnect;
    if (m_autoConnectServer.isEmpty() == false)
    {
        obj[QStringLiteral("auto_connect_server")] = m_autoConnectServer;
    }
    obj[QStringLiteral("notifications")] = m_notifications;
    obj[QStringLiteral("recent_connections_count")] = m_recentConnectionsCount;
    obj[QStringLiteral("start_hidden")] = m_startHidden;
    obj[QStringLiteral("close_to_tray")] = m_closeToTray;
    obj[QStringLiteral("show_location_picker")] = m_showLocationPicker;
    obj[QStringLiteral("show_favorites_dropdown")] = m_showFavoritesDropdown;
    obj[QStringLiteral("show_countries_sidebar")] = m_showCountriesSidebar;
    obj[QStringLiteral("show_quick_settings")] = m_showQuickSettings;
    obj[QStringLiteral("quick_settings")] = QJsonArray::fromStringList(m_quickSettings);
    obj[QStringLiteral("favorites_enabled")] = m_favoritesEnabled;
    if (m_lastSeenVersion.isEmpty() == false)
    {
        obj[QStringLiteral("last_seen_version")] = m_lastSeenVersion;
    }
    obj[QStringLiteral("check_for_updates")] = m_checkForUpdates;
    obj[QStringLiteral("log_to_file")] = m_logToFile;
    obj[QStringLiteral("verbose_logging")] = m_verboseLogging;
    obj[QStringLiteral("split_view_countries_ratio")] = m_splitViewCountriesRatio;
    obj[QStringLiteral("globe_animation")] = globeAnimationName(m_globeAnimation);
    obj[QStringLiteral("globe_pause_when_unfocused")] = m_globePauseWhenUnfocused;

    obj[QStringLiteral("theme")] = themeName(m_theme);

    return JsonFile::write(configFile(), QJsonDocument(obj));
}

bool AppConfig::autoConnect() const { return m_autoConnect; }

QString AppConfig::autoConnectServer() const { return m_autoConnectServer; }

void AppConfig::setAutoConnect(const bool value)
{
    if (m_autoConnect == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: auto_connect = ") + boolText(value));
    m_autoConnect = value;
    (void)save();
}

void AppConfig::setAutoConnectServer(const QString& value)
{
    if (m_autoConnectServer == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: auto_connect_server = ") + value);
    m_autoConnectServer = value;
    (void)save();
}

bool AppConfig::notifications() const { return m_notifications; }

void AppConfig::setNotifications(const bool value)
{
    if (m_notifications == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: notifications = ") + boolText(value));
    m_notifications = value;
    (void)save();
}

int AppConfig::recentConnectionsCount() const { return m_recentConnectionsCount; }

void AppConfig::setRecentConnectionsCount(const int value)
{
    if (m_recentConnectionsCount == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: recent_connections_count = ") + QString::number(qMax(0, value)));
    m_recentConnectionsCount = qMax(0, value);
    (void)save();
}

bool AppConfig::startHidden() const { return m_startHidden; }

void AppConfig::setStartHidden(const bool value)
{
    if (m_startHidden == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: start_hidden = ") + boolText(value));
    m_startHidden = value;
    (void)save();
}

double AppConfig::splitViewCountriesRatio() const { return m_splitViewCountriesRatio; }

void AppConfig::setSplitViewCountriesRatio(const double value)
{
    const double clamped = std::clamp(value, SPLIT_RATIO_MIN, SPLIT_RATIO_MAX);
    if (qFuzzyCompare(m_splitViewCountriesRatio, clamped)) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: split_view_countries_ratio = ") + QString::number(clamped));
    m_splitViewCountriesRatio = clamped;
    (void)save();
}

bool AppConfig::closeToTray() const { return m_closeToTray; }

void AppConfig::setCloseToTray(const bool value)
{
    if (m_closeToTray == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: close_to_tray = ") + boolText(value));
    m_closeToTray = value;
    (void)save();
}

AppConfig::GlobeAnimation AppConfig::globeAnimation() const { return m_globeAnimation; }

void AppConfig::setGlobeAnimation(const GlobeAnimation value)
{
    if (m_globeAnimation == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: globe_animation = ") + globeAnimationName(value));
    m_globeAnimation = value;
    (void)save();
}

bool AppConfig::globePauseWhenUnfocused() const { return m_globePauseWhenUnfocused; }

void AppConfig::setGlobePauseWhenUnfocused(const bool value)
{
    if (m_globePauseWhenUnfocused == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: globe_pause_when_unfocused = ") + boolText(value));
    m_globePauseWhenUnfocused = value;
    (void)save();
}

AppConfig::Theme AppConfig::theme() const { return m_theme; }

void AppConfig::setTheme(const Theme value)
{
    if (m_theme == value) return;
    m_theme = value;
    (void)save();
}

bool AppConfig::showLocationPicker() const { return m_showLocationPicker; }

void AppConfig::setShowLocationPicker(const bool value)
{
    if (m_showLocationPicker == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: show_location_picker = ") + boolText(value));
    m_showLocationPicker = value;
    (void)save();
}

bool AppConfig::showFavoritesDropdown() const { return m_showFavoritesDropdown; }

void AppConfig::setShowFavoritesDropdown(const bool value)
{
    if (m_showFavoritesDropdown == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: show_favorites_dropdown = ") + boolText(value));
    m_showFavoritesDropdown = value;
    (void)save();
}

bool AppConfig::showCountriesSidebar() const { return m_showCountriesSidebar; }

void AppConfig::setShowCountriesSidebar(const bool value)
{
    if (m_showCountriesSidebar == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: show_countries_sidebar = ") + boolText(value));
    m_showCountriesSidebar = value;
    (void)save();
}

bool AppConfig::showQuickSettings() const { return m_showQuickSettings; }

void AppConfig::setShowQuickSettings(const bool value)
{
    if (m_showQuickSettings == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: show_quick_settings = ") + boolText(value));
    m_showQuickSettings = value;
    (void)save();
}

QStringList AppConfig::quickSettings() const { return m_quickSettings; }

// static
QStringList AppConfig::defaultQuickSettings()
{
    return {
        QuickSettingIds::IPV6,
        QuickSettingIds::KILL_SWITCH,
        QuickSettingIds::NAT_TYPE,
        QuickSettingIds::NETSHIELD,
        QuickSettingIds::PORT_FORWARDING,
        QuickSettingIds::GLOBE_ANIMATION,
        QuickSettingIds::SHOW_COUNTRIES,
        QuickSettingIds::SHOW_LOCATION_PICKER,
        QuickSettingIds::SHOW_FAVORITES_DROPDOWN,
    };
}

void AppConfig::setQuickSettings(const QStringList& value)
{
    const QStringList normalized = normalizedQuickSettings(value);
    if (m_quickSettings == normalized) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: quick_settings = ") + normalized.join(QStringLiteral(", ")));
    m_quickSettings = normalized;
    (void)save();
}

bool AppConfig::favoritesEnabled() const { return m_favoritesEnabled; }

void AppConfig::setFavoritesEnabled(const bool value)
{
    if (m_favoritesEnabled == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: favorites_enabled = ") + boolText(value));
    m_favoritesEnabled = value;
    (void)save();
}

QString AppConfig::lastSeenVersion() const { return m_lastSeenVersion; }

void AppConfig::setLastSeenVersion(const QString& value)
{
    if (m_lastSeenVersion == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: last_seen_version = ") + value);
    m_lastSeenVersion = value;
    (void)save();
}

bool AppConfig::checkForUpdates() const { return m_checkForUpdates; }

void AppConfig::setCheckForUpdates(const bool value)
{
    if (m_checkForUpdates == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: check_for_updates = ") + boolText(value));
    m_checkForUpdates = value;
    (void)save();
}

bool AppConfig::logToFile() const { return m_logToFile; }

void AppConfig::setLogToFile(const bool value)
{
    if (m_logToFile == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: log_to_file = ") + boolText(value));
    m_logToFile = value;
    (void)save();
    FileLogger::instance().setEnabled(value);
}

bool AppConfig::verboseLogging() const { return m_verboseLogging; }

void AppConfig::setVerboseLogging(const bool value)
{
    if (m_verboseLogging == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: verbose_logging = ") + boolText(value));
    m_verboseLogging = value;
    (void)save();
    VerboseLog::setSetting(value);
}

void AppConfig::resetToDefaults()
{
    DBG_SETTINGS(QStringLiteral("AppConfig::resetToDefaults() - deleting config file and resetting all values"));

    // Delete the persisted file first so no stale data remains on disk.
    QFile::remove(configFile());

    // Reset every member to its compile-time default.
    m_autoConnect            = false;
    m_autoConnectServer      = QString();
    m_notifications          = true;
    m_recentConnectionsCount = DEFAULT_RECENT_CONNECTIONS_COUNT;
    m_startHidden            = false;
    m_closeToTray            = true;
    m_theme                  = Theme::System;
    m_showLocationPicker     = true;
    m_showFavoritesDropdown  = true;
    m_showCountriesSidebar   = true;
    m_showQuickSettings      = true;
    m_quickSettings          = defaultQuickSettings();
    m_favoritesEnabled       = true;
    m_lastSeenVersion        = QString();
    m_checkForUpdates        = true;
    m_logToFile              = false;
    m_verboseLogging         = false;
    m_splitViewCountriesRatio = SPLIT_RATIO_DEFAULT;
    m_globeAnimation          = GlobeAnimation::Auto;
    m_globePauseWhenUnfocused = false;
    FileLogger::instance().setEnabled(false);
    VerboseLog::setSetting(false);
}

