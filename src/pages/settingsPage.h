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
#include "../widgets/toggleWithStatus.h"

// ---------------------------------------------------------------------------
// SettingsPage
// ---------------------------------------------------------------------------
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(VpnManager* manager, NatPmpManager* natPmpManager, QWidget* parent = nullptr);
    void refresh();

signals:
    void recentConnectionsCleared();
    void locationPickerVisibilityChanged(bool visible);
    void globeSettingsChanged();
    void favoritesDropdownVisibilityChanged(bool visible);
    void favoritesEnabledChanged(bool enabled);
    void favoritesCleared();

private:
    // A simple on/off toggle row
    struct ToggleRow
    {
        QString cliKey;
        ToggleWithStatus* toggle = nullptr;
        // Value sent to the CLI (and expected when loading) for the ON state.
        // Most settings use "on"; kill-switch uses "standard".
        QString onValue = QStringLiteral("on");
    };

    // A combo-box row (multi-value setting)
    struct ComboRow
    {
        QString cliKey;
        QComboBox* combo = nullptr;
        QStringList cliValues; // parallel to combo items
    };

    VpnManager* m_manager;
    NatPmpManager* m_natPmpManager = nullptr;
    QList<ToggleRow> m_toggleRows;
    QList<ComboRow> m_comboRows;

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
    QComboBox* m_globeCombo = nullptr;
    QLabel* m_globeDescLabel = nullptr;
    ToggleWithStatus* m_globePauseToggle = nullptr;
    // Fills m_globeCombo; "Auto" is listed only when the desktop's
    // reduce-motion preference can be read (MotionPreference::Available).
    // Also updates the row's description, which explains Auto only then.
    void populateGlobeCombo();
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
                             std::function<void()> onAccept);
    static void addDivider(QVBoxLayout* layout, QWidget* parent);
    static QWidget* makePlusDivider(QWidget* parent);
    // A combo box whose popup items are drawn by the stylesheet, so the
    // dropdown looks the same under every Qt style.
    static QComboBox* makeComboBox(QWidget* parent);
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
