#pragma once

#include <QString>
#include <QStringList>
#include <QMap>

// ---------------------------------------------------------------------------
// AppConfig – persists app-level preferences to
//   ~/.config/ProtonVPN-Qt/app.json
// ---------------------------------------------------------------------------
class AppConfig
{
public:
    enum class Theme { System, Dark, Light };
    // The VPN page's background globe: Auto follows the desktop's reduce-motion
    // preference (see MotionPreference), On and Off override it.
    enum class GlobeAnimation { Auto, On, Off };

    static AppConfig &instance();

    // Directory holding app.json (differs under Flatpak, see appConfig.cpp).
    static QString configDir();

    // Logs every loaded setting via DBG_SETTINGS. Split out from load() so
    // callers can control exactly when it prints relative to other startup
    // diagnostics (see main.cpp).
    void logLoadedConfig() const;

    bool autoConnect() const;
    // Empty string means "Fastest Server"; "CC" means fastest in country;
    // "CC|city" means a specific city.  Stored as-is.
    QString autoConnectServer() const;
    // A location in autoConnectServer()'s format, and back: both parts empty
    // for the fastest server anywhere, the city empty for a country's fastest.
    struct ServerLocation
    {
        QString countryCode;
        QString city;
    };
    static QString autoConnectServerKey(const ServerLocation& location);
    static ServerLocation autoConnectServerLocation(const QString& key);
    bool notifications() const;
    int  recentConnectionsCount() const;
    bool startHidden() const;
    // True when closing the window hides to the tray instead of quitting.
    bool closeToTray() const;
    Theme theme() const;
    GlobeAnimation globeAnimation() const;
    // Stop the globe's idle spin while the window does not have focus.
    bool globePauseWhenUnfocused() const;
    // Single source of truth for the GlobeAnimation <-> config-string mapping.
    static QString globeAnimationName(GlobeAnimation value);
    // The reverse; anything unrecognized is Auto, the default.
    static GlobeAnimation globeAnimationFromName(const QString& name);
    bool showLocationPicker() const;
    bool showFavoritesDropdown() const;
    // Whether the VPN page shows the Countries list beside it in a wide
    // window, and its quick settings.
    bool showCountriesSidebar() const;
    bool showQuickSettings() const;
    // The settings the VPN page's quick settings show, by the names in
    // quickSettingIds.h: at most QUICK_SETTINGS_MAX, in no particular order.
    QStringList quickSettings() const;
    // The quick settings the app starts with, and goes back to on a restore.
    static QStringList defaultQuickSettings();
    bool favoritesEnabled() const;
    QString lastSeenVersion() const;
    // Whether the VPN page's tip pointing out the power button was dismissed.
    // The tip is for a first launch, so a config written before the setting
    // existed counts as dismissed.
    bool powerButtonHintShown() const;
    bool checkForUpdates() const;
    bool logToFile() const;
    // Verbose output (see VerboseLog in debug.h).
    bool verboseLogging() const;
    // Share of the width (0-1) the Countries list takes in the split view.
    double splitViewCountriesRatio() const;

    void setAutoConnect(bool value);
    void setAutoConnectServer(const QString& value);
    void setNotifications(bool value);
    void setRecentConnectionsCount(int value);
    void setStartHidden(bool value);
    void setCloseToTray(bool value);
    void setTheme(Theme value);
    void setGlobeAnimation(GlobeAnimation value);
    void setGlobePauseWhenUnfocused(bool value);
    void setShowLocationPicker(bool value);
    void setShowFavoritesDropdown(bool value);
    void setShowCountriesSidebar(bool value);
    void setShowQuickSettings(bool value);
    // Drops repeats and anything past QUICK_SETTINGS_MAX.
    void setQuickSettings(const QStringList& value);
    void setFavoritesEnabled(bool value);
    void setLastSeenVersion(const QString& value);
    void setPowerButtonHintShown(bool value);
    void setCheckForUpdates(bool value);
    void setLogToFile(bool value);
    void setVerboseLogging(bool value);
    // Clamped to [SPLIT_RATIO_MIN, SPLIT_RATIO_MAX].
    void setSplitViewCountriesRatio(double value);

    // The Countries list takes at most a third of the width, which is also
    // where it starts.
    static constexpr double SPLIT_RATIO_MIN     = 0.2;
    static constexpr double SPLIT_RATIO_MAX     = 1.0 / 3.0;
    static constexpr double SPLIT_RATIO_DEFAULT = SPLIT_RATIO_MAX;

    // As many as the quick settings show without scrolling.
    static constexpr int QUICK_SETTINGS_MAX = 12;

    // Resets every setting to its compile-time default and deletes the config
    // file. The in-memory state is usable immediately; the file will not be
    // recreated until the next call to a setter (which triggers save()).
    void resetToDefaults();

private:
    AppConfig();
    void load();
    bool save() const;

    // Single source of truth for the Theme <-> config-string mapping.
    static QString themeName(Theme theme);

    bool m_autoConnect    = false;
    QString m_autoConnectServer;
    bool m_notifications  = true;
    int  m_recentConnectionsCount = 5;
    bool m_startHidden = false;
    bool m_closeToTray = true;
    double m_splitViewCountriesRatio = SPLIT_RATIO_DEFAULT;
    Theme m_theme = Theme::System;
    GlobeAnimation m_globeAnimation = GlobeAnimation::Auto;
    bool m_globePauseWhenUnfocused = false;
    bool m_showLocationPicker = true;
    bool m_showFavoritesDropdown = true;
    bool m_showCountriesSidebar = true;
    bool m_showQuickSettings = true;
    QStringList m_quickSettings = defaultQuickSettings();
    bool m_favoritesEnabled = true;
    QString m_lastSeenVersion;
    bool m_powerButtonHintShown = false;
    bool m_checkForUpdates = true;
    bool m_logToFile = false;
    bool m_verboseLogging = false;
};
