#pragma once

#include <QCloseEvent>
#include <QDialog>
#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QTimer>
#include <QStackedWidget>
#include <QToolButton>
#include <QSystemTrayIcon>
#include <QMenu>
#include "vpnManager.h"
#include "widgets/gripSplitter.h"

class QNetworkAccessManager;

class VpnPage;
class LoginPage;
class CountriesPage;
class AccountPage;
class NotInstalledPage;
class SettingsPage;
class CliNotRespondingPage;
#ifdef QT_DEBUG
class DebugPage;
#endif

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    VpnManager* manager() const { return m_manager; }

private:
    enum class Page
    {
        Loading = 0,
        NotInstalled,
        Login,
        Vpn,
        Countries,
        Account,
        Settings,
        CliNotResponding,
#ifdef QT_DEBUG
        Debug,
#endif
    };

    VpnManager* m_manager;

    QWidget* m_sidebar;
    QFrame* m_sidebarDivider;
    QStackedWidget* m_stack;

    QToolButton* m_logoBtn;
    QToolButton* m_countriesNavBtn;
    QToolButton* m_accountNavBtn;
    QToolButton* m_settingsNavBtn;
#ifdef QT_DEBUG
    QToolButton* m_debugNavBtn = nullptr;
    QToolButton* m_loginDebugBtn = nullptr;
#endif

    NotInstalledPage* m_notInstalledPage;
    LoginPage* m_loginPage;
    VpnPage* m_vpnPage;
    CountriesPage* m_countriesPage;
    AccountPage* m_accountPage;
    SettingsPage* m_settingsPage;
    CliNotRespondingPage* m_cliNotRespondingPage;
#ifdef QT_DEBUG
    DebugPage* m_debugPage;
#endif

    // Split view: in a wide enough window the Countries page is shown beside
    // the VPN page, and on its own page only while opened from the nav (see
    // placeCountriesPage()). Each lives in a host widget so the stack indices
    // never shift when it moves.
    GripSplitter* m_homeSplitter = nullptr; // stack slot for Page::Vpn
    QWidget*   m_countriesHost  = nullptr; // stack slot for Page::Countries
    QTimer*    m_splitSaveTimer = nullptr; // saves the divider position after a drag
    bool       m_splitView      = false;
    // Turns the split view on or off for the window's width and the Show
    // Countries setting.
    void updateSplitView();
    void applySplitView(bool split);
    // Keeps the Countries list in the split view to its share of the width.
    void limitCountriesWidth();
    // Moves the Countries page into the split view or back to its own stack
    // slot, for the split view and the page shown.
    void placeCountriesPage();
    // Places the split view divider at the saved Countries share of the width.
    void applySplitRatio();

    void showPage(Page page);
    // Startup failure pages, also reachable from the Debug page for testing.
    void showNotInstalled();     // the protonvpn CLI could not be found
    void showCliNotResponding(); // the startup login check timed out
    void repositionLoginDebugBtn();
    void setupSidebar();
    void refreshIcons();
    void setNavActive(const QToolButton* btn);
    void startupCheck() const;
    void checkForUpdates();
    void updateTrayIcon(VpnState state);
    void updateTrayTooltipAndAction(VpnState state) const;
    void notifyStateTransition(VpnState state);
    // Prompts before quitting with an active connection.  Returns true when the
    // caller should proceed with the quit.
    bool confirmQuit();
    void sendNotification(const QString& title, const QString& message) const;
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void maybeShowWhatsNew();
    // Shows the VPN page's first-launch tip on the power button once the page
    // is up and the What's New dialog, which a first launch also opens, is
    // closed.
    void maybeShowPowerButtonHint();

    QNetworkAccessManager* m_networkManager = nullptr;
    // Null when the desktop provides no system tray; every use must be guarded.
    QSystemTrayIcon* m_trayIcon = nullptr;
    QAction* m_trayConnectAction = nullptr;
    bool m_startupAutoConnectPending = false; // auto-connect if the first known state is Disconnected
    VpnState m_lastNotifiedState = VpnState::Unknown;
    bool m_whatsNewShown = false; // guard so we only show the dialog once per launch
    QPointer<QDialog> m_whatsNewDialog;
#ifdef QT_DEBUG
    Page m_preDebugPage = Page::Login; // page to return to when leaving Debug
    bool m_signinPreview = false;      // the login page shows a Debug page preview
    // The connection state as VpnManager last reported it, which the Debug
    // page's Hide Error puts back on the VPN page.
    VpnState m_lastReportedState = VpnState::Unknown;
    QString  m_lastReportedInfo;
#endif
};
