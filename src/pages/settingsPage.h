#pragma once

#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <functional>
#include <utility>
#include "../vpnManager.h"
#include "../cli/natpmpManager.h"
#include "../dialogs/aboutDialog.h"
#include "../widgets/quickSettingSource.h"
#include "../widgets/toggleWithStatus.h"

class CliSettingControls;
class GlobeAnimationCombo;

// ---------------------------------------------------------------------------
// SettingsPage
// ---------------------------------------------------------------------------
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(VpnManager* manager, NatPmpManager* natPmpManager, QWidget* parent = nullptr);
    void refresh();
    // Every setting on this page, for the VPN page's quick settings to show
    // the ones chosen with each row's right-click menu.
    [[nodiscard]] const QList<QuickSettingSource>& quickSettingSources() const { return m_quickSettingSources; }

signals:
    void recentConnectionsCleared();
    void locationPickerVisibilityChanged(bool visible);
    void globeSettingsChanged();
    void favoritesDropdownVisibilityChanged(bool visible);
    void favoritesEnabledChanged(bool enabled);
    void favoritesCleared();
    void countriesSidebarVisibilityChanged(bool visible);
    // Which quick settings to show changed, or whether to show them at all.
    void quickSettingsChanged();

private:
    VpnManager* m_manager;
    NatPmpManager* m_natPmpManager = nullptr;
    // The VPN tab's toggles and combo boxes made by makeToggleRow() and
    // makeComboRow().
    CliSettingControls* m_cliControls = nullptr;

    // Custom DNS widgets
    ToggleWithStatus* m_dnsToggle = nullptr;
    QLineEdit* m_dnsEdit = nullptr;
    QWidget* m_dnsAddrRow = nullptr; // the server field and Apply, shown while custom DNS is on
    QPushButton* m_dnsApplyBtn = nullptr;

    // Port forwarding toggle
    ToggleWithStatus* m_portForwardingToggle = nullptr;
    QWidget* m_settingsPortRow = nullptr;
    QLabel*  m_settingsPortLabel = nullptr;

    // Kill switch toggle + collapsible radio-button sub-panel
    ToggleWithStatus* m_killSwitchToggle   = nullptr;
    QWidget*      m_killSwitchSubPanel = nullptr;
    // True while an applyConfigValueAndReconnect() sequence is in flight;
    // keeps the whole VPN card disabled through the Disconnected interim.
    bool m_sequencePending = false;

    // Auto-start (XDG autostart .desktop file)
    ToggleWithStatus* m_autoStartToggle = nullptr;
    QWidget* m_autoStartRow = nullptr;

    // Auto-connect on startup (shown only when auto-start is on)
    ToggleWithStatus* m_autoConnectToggle = nullptr;
    QWidget* m_autoConnectRow = nullptr;

    // Auto-connect server dropdown (shown only when auto-connect is on)
    QWidget*   m_autoConnectServerRow   = nullptr;
    QComboBox* m_autoConnectServerCombo = nullptr;

    // Desktop notifications
    ToggleWithStatus* m_notificationsToggle = nullptr;

    // Theme selector combo box
    QComboBox* m_themeCombo = nullptr;
    GlobeAnimationCombo* m_globeCombo = nullptr;
    ToggleWithStatus* m_globePauseToggle = nullptr;
    // Pause When Unfocused only applies while the globe can show.
    void updateGlobePauseToggle() const;
    [[nodiscard]] static QString globeDescription();

    // Recent connections count (0 = disabled)
    class NumberSpinner* m_recentConnectionsSpinBox = nullptr;

    // "Clear history" row – shown only when history is non-empty
    QWidget* m_clearRecentRow = nullptr;

    // "Clear favorites" row – shown only when favorites is non-empty
    QWidget* m_clearFavoritesRow = nullptr;

    // Clickable "About" row in the App tab (event filter handles the click)
    QWidget* m_aboutRow = nullptr;

    // "Show Favorites Dropdown" toggle in the Appearance tab – disabled when
    // the Favorites system is turned off from the App tab.
    QWidget*          m_showFavoritesDropdownRow    = nullptr;
    ToggleWithStatus* m_showFavoritesDropdownToggle = nullptr;

    // Plus Members Only section (VPN tab)
    QWidget* m_plusSection  = nullptr;
    QWidget* m_plusDivider  = nullptr;

    // The card widget that wraps all VPN-tab settings - disabled en-masse
    // while the VPN is connecting or disconnecting.
    QWidget* m_vpnCard = nullptr;

    // Plus Members Only section (App tab)
    QWidget* m_appPlusSection = nullptr;
    QWidget* m_appPlusDivider = nullptr;

    // Installed CLI version (cached when cliVersionReady fires)
    QString m_installedCliVersion;

    QPushButton* m_refreshBtn;
    QLabel* m_statusLabel;
    QTimer* m_spinnerTimer;
    int m_spinnerFrame = 0;
    bool m_loading = false;

    // The VPN tab widget – watched via eventFilter to keep m_refreshBtn
    // positioned as a floating overlay in its top-right corner.
    QWidget* m_vpnTabWidget = nullptr;

    // Tab builders - one per QTabWidget page, called from the constructor.
    void buildAppTab(QTabWidget* tabs);
    void buildAppearanceTab(QTabWidget* tabs);
    void buildVpnTab(QTabWidget* tabs);
    // Scroll area + "infoCard" container shared by the tabs.
    static std::pair<QWidget*, QVBoxLayout*> makeTabCard(QWidget* tabPage);
    // Scroll area + transparent container for a tab laid out as titled
    // sections, each in its own card (the App and Appearance tabs).
    static std::pair<QWidget*, QVBoxLayout*> makeSectionedTab(QWidget* tabPage);
    // Fills a tab with a scroll area showing `content`, under the tab's own
    // layout (the VPN tab adds its status line to that layout).
    static void addTabScrollArea(QWidget* tabPage, QWidget* content);
    // Adds an uppercase section title, preceded by a divider unless it is the
    // first thing in the layout.
    static void addSectionHeader(QWidget* parent, QVBoxLayout* layout, const QString& title);
    // Adds an empty "infoCard" to hold one section's rows.
    static std::pair<QWidget*, QVBoxLayout*> addSectionCard(QWidget* parent, QVBoxLayout* layout);

    QList<QuickSettingSource> m_quickSettingSources;
    // Lists a setting for the quick settings, and gives its row the
    // right-click menu that adds it to them or takes it out.
    void addQuickSetting(const QuickSettingSource& source);

    // Helpers
    QWidget* makeToggleRow(QWidget* parent, const QString& label, const QString& desc,
                           const QString& cliKey,
                           const QString& onValue = QStringLiteral("on"),
                           bool requiresReconnect = false);
    QWidget* makeComboRow(QWidget* parent, const QString& label, const QString& desc,
                          const QString& cliKey, const QStringList& labels,
                          const QStringList& cliValues, bool requiresReconnect = false);
    // Shows the standard "Apply & Reconnect" dialog for any setting
    // that cannot be changed while the VPN is active.  onAccept is called if
    // the user clicks the primary button; nothing extra is called on dismiss.
    void showReconnectDialog(const QString& settingLabel,
                             const std::function<void()>& onAccept);
    static void addDivider(QVBoxLayout* layout, QWidget* parent);
    void updatePlusSectionState() const;
    void maybeWarnReconnect(const QString& cliOutput);

    // Auto-start helpers
    static QString autoStartFilePath();
    static bool autoStartEnabled();
    static bool setAutoStart(bool enable, QString& errorOut);
    void updateAutoConnectRowVisibility() const;
    void updateAutoConnectServerRow() const;
    void populateAutoConnectServerCombo() const;

    void onSettingsReady(const QMap<QString, QString>& settings);
    void setLoading(bool loading);
    bool eventFilter(QObject* obj, QEvent* event) override;
};
