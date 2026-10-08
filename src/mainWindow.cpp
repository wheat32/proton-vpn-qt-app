#include <QApplication>
#include <QButtonGroup>
#include <QFile>
#include <QFrame>
#include <QGuiApplication>
#include <QStyleHints>
// ReSharper disable once CppUnusedIncludeDirective
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QSvgWidget>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVersionNumber>
#include <utility>
#include "appConfig.h"
#include "connectionHistory.h"
#include "debug.h"
#include "dialogs/quitDialog.h"
#include "dialogs/updateAvailableDialog.h"
#include "dialogs/whatsNewDialog.h"
#include "geoUtils.h"
#include "mainWindow.h"
#include "themeManager.h"
#include "pages/accountPage.h"
#include "pages/cliNotRespondingPage.h"
#include "pages/countriesPage.h"
#include "pages/loginPage.h"
#include "pages/notInstalledPage.h"
#include "pages/settingsPage.h"
#include "pages/vpnPage.h"
#ifdef QT_DEBUG
#include "pages/debugPage.h"
#endif

namespace
{
// Window dimensions
constexpr int WINDOW_ICON_SIZE       = 64;
constexpr int MIN_WINDOW_WIDTH       = 460;
constexpr int MIN_WINDOW_HEIGHT      = 580;
constexpr int INITIAL_WINDOW_WIDTH   = 660;
constexpr int INITIAL_WINDOW_HEIGHT  = 600;

// Split view: from this window width on, the Countries list is shown beside
// the VPN page instead of on its own page.
constexpr int SPLIT_VIEW_MIN_WINDOW_WIDTH = 1200;
// Narrowest the two sides of the split view can be dragged to. Together they
// must fit in the stack at SPLIT_VIEW_MIN_WINDOW_WIDTH. The VPN page keeps
// room for its quick settings.
constexpr int SPLIT_COUNTRIES_MIN_WIDTH   = 360;
constexpr int SPLIT_VPN_MIN_WIDTH         = VpnPage::SPLIT_MIN_WIDTH;
// How long after the last divider drag its position is saved.
constexpr int SPLIT_SAVE_DELAY_MS         = 500;

// Sidebar layout
constexpr int SIDEBAR_WIDTH          = 64;
constexpr int SIDEBAR_DIVIDER_WIDTH  = 1;
constexpr int SIDEBAR_MARGIN         = 8;
constexpr int SIDEBAR_LAYOUT_SPACING = 4;
constexpr int SIDEBAR_LOGO_SPACING   = 12;

// Button sizes
constexpr int LOGO_BTN_ICON_SIZE     = 40;
constexpr int LOGO_BTN_SIZE          = 56;
constexpr int NAV_ICON_SIZE          = 24;
constexpr int NAV_BTN_SIZE           = 48;

// Proton dark-theme nav tint color (#1a1a2e)
constexpr int DARK_BG_R = 0x1a;
constexpr int DARK_BG_G = 0x1a;
constexpr int DARK_BG_B = 0x2e;

// Notifications and tray
constexpr int NOTIFICATION_ICON_SIZE   = 64;
constexpr int NOTIFICATION_DURATION_MS = 4000;
constexpr int TRAY_ICON_SIZE           = 22;

// Update check
constexpr int UPDATE_CHECK_DELAY_MS   = 3000;
constexpr int UPDATE_CHECK_TIMEOUT_MS = 10000;
constexpr const char* UPDATE_VERSION_URL =
    "https://raw.githubusercontent.com/wheat32/proton-vpn-qt-app/main/src/version.json";

// Startup
constexpr int WHATS_NEW_DELAY_MS = 400;

// Renders the SVG at `path` into a QIcon of `size`.
// When `tintForTheme` is true (used for monochrome utility icons), the result
// is tinted white on dark backgrounds and dark navy on light backgrounds so
// the icon is always legible.  Pass false for branded/colored logos that
// should be rendered with their own SVG colors unchanged.
QIcon svgNavIcon(const QString& path, const QSize& size = {NAV_ICON_SIZE, NAV_ICON_SIZE}, bool tintForTheme = true)
{
    if (tintForTheme == false)
    {
        return QIcon(GeoUtils::svgPixmap(path, size.width(), size.height()));
    }

    const QColor tintColor = ThemeManager::isDark() == true
                                 ? QColor(Qt::white)
                                 : QColor(DARK_BG_R, DARK_BG_G, DARK_BG_B);
    return QIcon(GeoUtils::svgPixmap(path, size.width(), size.height(), tintColor));
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("ProtonVPN"));
    setWindowIcon(svgNavIcon(QStringLiteral(":/assets/proton-vpn-sign.svg"), {WINDOW_ICON_SIZE, WINDOW_ICON_SIZE}, false));
    setMinimumSize(MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
    resize(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT);

    m_manager = new VpnManager(this);
    m_networkManager = new QNetworkAccessManager(this);

    // Root layout: sidebar + content
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Sidebar
    m_sidebar = new QWidget(this);
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setFixedWidth(SIDEBAR_WIDTH);
    setupSidebar();
    m_sidebar->setEnabled(false); // disabled until startup checks complete
    rootLayout->addWidget(m_sidebar);

    // Vertical divider
    m_sidebarDivider = new QFrame(this);
    m_sidebarDivider->setFrameShape(QFrame::VLine);
    m_sidebarDivider->setFixedWidth(SIDEBAR_DIVIDER_WIDTH);
    m_sidebarDivider->setObjectName(QStringLiteral("sidebarDivider"));
    rootLayout->addWidget(m_sidebarDivider);

    // Stacked content
    m_stack = new QStackedWidget(this);
    rootLayout->addWidget(m_stack);

    // Loading page (index 0)
    auto* loadingPage = new QWidget();
    auto* loadingLayout = new QVBoxLayout(loadingPage);
    loadingLayout->setAlignment(Qt::AlignCenter);
    auto* loadingLabel = new QLabel(tr("Starting\u2026"), loadingPage);
    loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLayout->addWidget(loadingLabel);
    m_stack->addWidget(loadingPage); // index 0 = Loading

    // Not installed page (index 1)
    m_notInstalledPage = new NotInstalledPage();
    m_stack->addWidget(m_notInstalledPage); // index 1
    connect(m_notInstalledPage, &NotInstalledPage::recheckRequested, this, [this]()
    {
        showPage(Page::Loading);
        m_manager->checkInstalled();
    });

    // Login page (index 2)
    m_loginPage = new LoginPage();
    m_stack->addWidget(m_loginPage); // index 2
    // Each answer shows as in progress until the CLI asks for the next thing
    // (signinPrompt) or sign-in ends (loginFinished). Retrying after the CLI
    // gives up, with the credentials it was started with, is the sign-in
    // flow's job (cli/signin/).
    const auto answering = [this]()
    {
        m_loginPage->setLoading(true);
        m_loginPage->setError(QString());
    };
    connect(m_loginPage, &LoginPage::loginRequested, this, [this, answering](const QString& u, const QString& p)
    {
        answering();
        m_manager->login(u, p);
    });
    connect(m_loginPage, &LoginPage::twoFASubmitted, this, [this, answering](const QString& code)
    {
        answering();
        m_manager->submit2FA(code);
    });
    connect(m_loginPage, &LoginPage::securityKeyPinSubmitted, this, [this, answering](const QString& pin)
    {
        answering();
        m_manager->submitSecurityKeyPin(pin);
    });
    connect(m_loginPage, &LoginPage::securityKeyRetryRequested, this, [this, answering]()
    {
        answering();
        m_manager->retrySecurityKey();
    });
    connect(m_loginPage, &LoginPage::useAuthenticatorCodeRequested, this, [this, answering]()
    {
        answering();
        m_manager->useAuthenticatorCode();
    });
    connect(m_loginPage, &LoginPage::loginCancelRequested, this, [this]()
    {
        m_manager->cancelLogin();
        m_loginPage->reset();
#ifdef QT_DEBUG
        // Going back from a Debug page preview returns to the Debug page.
        if (m_signinPreview == true)
        {
            showPage(Page::Debug);
        }
#endif
    });

    // VPN page (index 3). Hosted in a splitter so a wide window can show the
    // Countries page beside it; the splitter holds only the VPN page otherwise.
    m_vpnPage = new VpnPage(m_manager);
    m_homeSplitter = new GripSplitter();
    m_homeSplitter->setChildrenCollapsible(false);
    m_homeSplitter->addWidget(m_vpnPage);
    m_stack->addWidget(m_homeSplitter); // index 3

    // Double-clicking the divider puts it back at its default position.
    connect(m_homeSplitter, &GripSplitter::handleDoubleClicked, this, [this]()
    {
        m_splitSaveTimer->stop(); // a drag just before must not overwrite the reset
        AppConfig::instance().setSplitViewCountriesRatio(AppConfig::SPLIT_RATIO_DEFAULT);
        applySplitRatio();
    });

    m_splitSaveTimer = new QTimer(this);
    m_splitSaveTimer->setSingleShot(true);
    m_splitSaveTimer->setInterval(SPLIT_SAVE_DELAY_MS);
    connect(m_splitSaveTimer, &QTimer::timeout, this, [this]()
    {
        const QList<int> sizes = m_homeSplitter->sizes();
        if (m_splitView == false || sizes.size() != 2) return;
        const int total = sizes.at(0) + sizes.at(1);
        if (total <= 0) return;
        AppConfig::instance().setSplitViewCountriesRatio(static_cast<double>(sizes.at(0)) / total);
    });
    // splitterMoved fires continuously during a drag; save once it settles.
    connect(m_homeSplitter, &QSplitter::splitterMoved, m_splitSaveTimer, qOverload<>(&QTimer::start));
    connect(m_vpnPage, &VpnPage::connectRequested, m_manager,
            [this](const QString& country, const QString& city)
            {
                if (country.isEmpty() == false && city.isEmpty() == false)
                {
                    const QString name = GeoUtils::countryCodeToName(country);
                    ConnectionHistory::instance().record(country, name, city);
                }
                m_manager->connectVpn(country, city);
            });
    connect(m_vpnPage, &VpnPage::disconnectRequested, m_manager, &VpnManager::disconnectVpn);
    connect(m_vpnPage, &VpnPage::signOutRequested, this, [this]()
    {
        m_manager->signOut();
    });
    connect(m_vpnPage, &VpnPage::changeCountryRequested, this, [this]()
    {
        // In the split view the Countries list is already beside the picker.
        if (m_splitView == false)
        {
            showPage(Page::Countries);
        }
    });

    // Countries page (index 4)
    m_countriesPage = new CountriesPage(m_manager);
    m_countriesHost = new QWidget();
    QVBoxLayout* countriesHostLayout = new QVBoxLayout(m_countriesHost);
    countriesHostLayout->setContentsMargins(0, 0, 0, 0);
    countriesHostLayout->addWidget(m_countriesPage);
    m_stack->addWidget(m_countriesHost); // index 4
    connect(m_countriesPage, &CountriesPage::connectRequested, this,
            [this](const QString& country, const QString& city)
            {
                m_vpnPage->notifyExternalConnect(city);
                if (country.isEmpty() == false && city.isEmpty() == false)
                {
                    const QString name = GeoUtils::countryCodeToName(country);
                    ConnectionHistory::instance().record(country, name, city);
                }
                m_manager->connectVpn(country, city);
                showPage(Page::Vpn);
            });

    // Account page (index 5)
    m_accountPage = new AccountPage(m_manager);
    m_stack->addWidget(m_accountPage); // index 5
    connect(m_accountPage, &AccountPage::signOutRequested, this, [this]()
    {
        m_manager->signOut();
    });

    // Settings page (index 6)
    m_settingsPage = new SettingsPage(m_manager, m_vpnPage->natPmpManager());
    m_stack->addWidget(m_settingsPage); // index 6
    connect(m_settingsPage, &SettingsPage::recentConnectionsCleared,
            m_vpnPage, &VpnPage::refreshRecentPicker);
    connect(m_settingsPage, &SettingsPage::locationPickerVisibilityChanged,
            m_vpnPage, &VpnPage::setLocationPickerVisible);
    connect(m_settingsPage, &SettingsPage::globeSettingsChanged,
            m_vpnPage, &VpnPage::applyGlobeSettings);
    connect(m_settingsPage, &SettingsPage::favoritesDropdownVisibilityChanged,
            m_vpnPage, &VpnPage::setFavoritesDropdownVisible);
    connect(m_settingsPage, &SettingsPage::favoritesEnabledChanged,
            m_vpnPage, &VpnPage::setFavoritesEnabled);
    connect(m_settingsPage, &SettingsPage::favoritesCleared,
            m_vpnPage, &VpnPage::refreshFavoritesPicker);

    // The VPN page's quick settings are copies of the Settings page's
    // controls, chosen there with each setting's right-click menu.
    m_vpnPage->setQuickSettingSources(m_settingsPage->quickSettingSources());
    connect(m_settingsPage, &SettingsPage::quickSettingsChanged,
            m_vpnPage, &VpnPage::updateQuickSettings);
    connect(m_settingsPage, &SettingsPage::countriesSidebarVisibilityChanged,
            this, &MainWindow::updateSplitView);

    // CLI not responding page (index 7)
    m_cliNotRespondingPage = new CliNotRespondingPage();
    m_stack->addWidget(m_cliNotRespondingPage); // index 7
    connect(m_cliNotRespondingPage, &CliNotRespondingPage::retryRequested, this, [this]()
    {
        showPage(Page::Loading);
        m_manager->checkLoginStatus();
    });

#ifdef QT_DEBUG
    // Debug page (index 8) – only present in debug builds
    m_debugPage = new DebugPage();
    m_stack->addWidget(m_debugPage); // index 8
    connect(m_debugPage, &DebugPage::notInstalledPageRequested,
            this, &MainWindow::showNotInstalled);
    connect(m_debugPage, &DebugPage::cliNotRespondingPageRequested,
            this, &MainWindow::showCliNotResponding);
    connect(m_debugPage, &DebugPage::homeWarningRequested,
            m_vpnPage, &VpnPage::addWarning);
    // Shown on the home screen only: VpnManager keeps its real state, and the
    // next real change replaces the error.
    connect(m_debugPage, &DebugPage::homeErrorRequested, this, [this](const QString& details)
    {
        m_vpnPage->onStateChanged(VpnState::Error, details);
    });
    connect(m_debugPage, &DebugPage::homeErrorHidden, this, [this]()
    {
        m_vpnPage->onStateChanged(m_lastReportedState, m_lastReportedInfo);
    });
    connect(m_debugPage, &DebugPage::signinScreenRequested,
            this, [this](const SigninPrompt prompt, const QString& error)
    {
        // A preview only: with no sign-in in progress, its buttons do nothing
        // but Go Back, which returns to the Debug page.
        m_loginPage->reset();
        m_loginPage->showSigninPreview(prompt, error);
        m_signinPreview = true;
        showPage(Page::Login);
    });

    // Floating debug button shown at bottom-left during login (sidebar is hidden then)
    m_loginDebugBtn = new QToolButton(this);
    m_loginDebugBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/bug.svg"), {NAV_ICON_SIZE, NAV_ICON_SIZE}));
    m_loginDebugBtn->setIconSize({NAV_ICON_SIZE, NAV_ICON_SIZE});
    m_loginDebugBtn->setFixedSize(NAV_BTN_SIZE, NAV_BTN_SIZE);
    m_loginDebugBtn->setToolTip(tr("Debug"));
    m_loginDebugBtn->setObjectName(QStringLiteral("navButton"));
    m_loginDebugBtn->setCursor(Qt::PointingHandCursor);
    m_loginDebugBtn->setVisible(false);
    // Click target is wired/rewired in showPage() depending on current context.
#endif

    // Keep the recent picker in sync with any history change (record, clear, trim).
    connect(&ConnectionHistory::instance(), &ConnectionHistory::changed,
            m_vpnPage, &VpnPage::refreshRecentPicker);

    // VpnManager signals
    connect(m_manager, &VpnManager::connectionCityKnown,
            m_vpnPage, &VpnPage::onStatusCityKnown);

    // Show CLI version-mismatch banner on the login page as well as the VPN page.
    connect(m_manager, &VpnManager::cliVersionReady,
            m_loginPage, &LoginPage::onCliVersionReady);

    connect(m_manager, &VpnManager::installedResult, this, [this](bool installed)
    {
        if (installed == false)
        {
            showNotInstalled();
        }
        else
        {
            m_manager->checkLoginStatus();
        }
    });

    connect(m_manager, &VpnManager::loginStatusResult, this, [this](bool loggedIn, const QString& username)
    {
        Q_UNUSED(username)
        if (loggedIn)
        {
            m_sidebar->setEnabled(true);
            showPage(Page::Vpn);
            m_manager->fetchCountries();
            // Mark that we should auto-connect once the initial status check resolves.
            if (AppConfig::instance().autoConnect() == true)
            {
                m_startupAutoConnectPending = true;
            }
        }
        else
        {
            m_sidebar->setEnabled(false);
            showPage(Page::Login);
        }
        // Delay slightly so the page transition is visible before the dialog pops.
        QTimer::singleShot(WHATS_NEW_DELAY_MS, this, &MainWindow::maybeShowWhatsNew);
    });

    connect(m_manager, &VpnManager::loginCheckTimedOut, this, &MainWindow::showCliNotResponding);

    connect(m_manager, &VpnManager::signinPrompt, m_loginPage, &LoginPage::showSigninPrompt);

    connect(m_manager, &VpnManager::loginFinished, this, [this](bool ok, const QString& error)
    {
        m_loginPage->setLoading(false);
        if (ok)
        {
            m_loginPage->reset();
            m_sidebar->setEnabled(true);
            showPage(Page::Vpn);
            m_manager->fetchCountries();
        }
        else
        {
            m_loginPage->setError(error.isEmpty()
                                      ? tr("Login failed. Please check your credentials.")
                                      : error);
        }
    });

    // The CLI reported that the server rejected the session (from any command,
    // not just connect). Sign out so the user lands on the login page instead
    // of on a VPN page where every action fails.
    connect(m_manager, &VpnManager::sessionExpired, m_manager, &VpnManager::signOut);

    connect(m_manager, &VpnManager::signOutFinished, this, [this](bool)
    {
        // A startup auto-connect still waiting for its first Disconnected state
        // must not fire on the one signOut() announces next, while signed out.
        m_startupAutoConnectPending = false;
        m_loginPage->reset();
        m_sidebar->setEnabled(false);
        showPage(Page::Login);
    });

    connect(m_manager, &VpnManager::connectionStateChanged, this,
            [this](VpnState state, const QString& info)
            {
                m_vpnPage->onStateChanged(state, info);
                updateTrayIcon(state);
#ifdef QT_DEBUG
                m_lastReportedState = state;
                m_lastReportedInfo  = info;
#endif

                // Startup auto-connect, decided by the first state the app
                // learns: connect now if that is Disconnected; otherwise (the
                // VPN was already up when the app started, or the user got
                // there first) there is nothing to do. Either way the flag is
                // spent here - left set, it would fire on the next disconnect,
                // including one the user asked for.
                if (m_startupAutoConnectPending == true && state != VpnState::Unknown)
                {
                    m_startupAutoConnectPending = false;
                    if (state != VpnState::Disconnected) return;
                    const AppConfig::ServerLocation location =
                        AppConfig::autoConnectServerLocation(AppConfig::instance().autoConnectServer());
                    m_manager->startupAutoConnect(location.countryCode, location.city);
                }
            });

    // System tray icon.  Only created when the desktop actually provides a
    // tray: without this check, "Start Hidden" on a tray-less desktop launches
    // the app with no window and no icon, and the single-instance lock then
    // blocks a second launch.  Every m_trayIcon use is null-guarded.
    if (QSystemTrayIcon::isSystemTrayAvailable())
    {
        m_trayIcon = new QSystemTrayIcon(this);
        auto* trayMenu = new QMenu(this);
        trayMenu->addAction(tr("Show"), this, [this]()
        {
            showNormal();
            raise();
            activateWindow();
        });
        trayMenu->addSeparator();
        m_trayConnectAction = trayMenu->addAction(tr("Connect"), this, [this]()
        {
            const VpnState state = m_manager->currentState();
            if (state == VpnState::Connected)
            {
                m_manager->disconnectVpn();
            }
            else if (state == VpnState::Disconnected || state == VpnState::Error)
            {
                m_manager->connectVpn();
            }
        });
        trayMenu->addSeparator();
        trayMenu->addAction(tr("Quit"), this, [this]()
        {
            if (confirmQuit() == false) return;
            QApplication::quit();
        });
        m_trayIcon->setContextMenu(trayMenu);
        connect(m_trayIcon, &QSystemTrayIcon::activated, this,
                [this](const QSystemTrayIcon::ActivationReason reason)
                {
                    if (reason == QSystemTrayIcon::Trigger)
                    {
                        showNormal();
                        raise();
                        activateWindow();
                    }
                });
        updateTrayIcon(VpnState::Unknown);
        m_trayIcon->show();
    }
    else
    {
        DBG_APP(QStringLiteral("No system tray available - running without a tray icon."));
    }

    // Follow the desktop's light/dark switch while the theme setting is
    // "System".  The palette-driven parts of the UI (icons, logo, spinner)
    // already repaint on their own via QEvent::PaletteChange, but the app
    // stylesheet is only chosen inside ThemeManager::apply(), so without this
    // the QSS-driven surfaces - cards, popups, the login card - kept whichever
    // theme was current at launch.
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [](Qt::ColorScheme)
            {
                if (AppConfig::instance().theme() != AppConfig::Theme::System)
                    return; // the user pinned a theme - the desktop does not override it

                DBG_APP(QStringLiteral("System color scheme changed - reapplying theme."));
                ThemeManager::apply(AppConfig::Theme::System);
            });
#endif

    // Start
    showPage(Page::Loading);
    startupCheck();
    QTimer::singleShot(UPDATE_CHECK_DELAY_MS, this, &MainWindow::checkForUpdates);
}

void MainWindow::setupSidebar()
{
    auto* layout = new QVBoxLayout(m_sidebar);
    layout->setContentsMargins(0, SIDEBAR_MARGIN, 0, SIDEBAR_MARGIN);
    layout->setSpacing(SIDEBAR_LAYOUT_SPACING);
    layout->setAlignment(Qt::AlignHCenter);

    auto* btnGroup = new QButtonGroup(m_sidebar);
    btnGroup->setExclusive(true);

    // Logo button is part of the exclusive group so clicking it automatically
    // clears the checked state on all nav buttons.
    m_logoBtn = new QToolButton(m_sidebar);
    m_logoBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/proton-vpn-sign.svg"), {LOGO_BTN_ICON_SIZE, LOGO_BTN_ICON_SIZE}, false));
    m_logoBtn->setIconSize({LOGO_BTN_ICON_SIZE, LOGO_BTN_ICON_SIZE});
    m_logoBtn->setFixedSize(LOGO_BTN_SIZE, LOGO_BTN_SIZE);
    m_logoBtn->setToolTip(tr("VPN"));
    m_logoBtn->setCursor(Qt::PointingHandCursor);
    m_logoBtn->setObjectName(QStringLiteral("logoButton"));
    m_logoBtn->setCheckable(true);
    btnGroup->addButton(m_logoBtn);
    layout->addWidget(m_logoBtn, 0, Qt::AlignHCenter);
    connect(m_logoBtn, &QToolButton::clicked, this, [this]() { showPage(Page::Vpn); });

    layout->addSpacing(SIDEBAR_LOGO_SPACING);

    auto makeNavBtn = [&](const QString& tooltip, const QString& iconPath) -> QToolButton*
    {
        QToolButton* btn = new QToolButton(m_sidebar);
        btn->setToolTip(tooltip);
        btn->setIcon(svgNavIcon(iconPath, {NAV_ICON_SIZE, NAV_ICON_SIZE}));
        btn->setIconSize({NAV_ICON_SIZE, NAV_ICON_SIZE});
        btn->setFixedSize(NAV_BTN_SIZE, NAV_BTN_SIZE);
        btn->setCheckable(true);
        btn->setObjectName(QStringLiteral("navButton"));
        btn->setCursor(Qt::PointingHandCursor);
        btnGroup->addButton(btn);
        layout->addWidget(btn, 0, Qt::AlignHCenter);
        return btn;
    };

    m_countriesNavBtn = makeNavBtn(tr("Countries"), QStringLiteral(":/assets/server-smart-routing.svg"));
    m_accountNavBtn = makeNavBtn(tr("Account"), QStringLiteral(":/assets/person-lines-fill.svg"));

    connect(m_countriesNavBtn, &QToolButton::clicked, this, [this]() { showPage(Page::Countries); });
    connect(m_accountNavBtn, &QToolButton::clicked, this, [this]()
    {
        showPage(Page::Account);
        m_accountPage->refresh();
    });

    layout->addStretch();

    // Settings button pinned to the bottom of the sidebar
    m_settingsNavBtn = makeNavBtn(tr("Settings"), QStringLiteral(":/assets/gear.svg"));
    connect(m_settingsNavBtn, &QToolButton::clicked, this, [this]()
    {
        showPage(Page::Settings);
        m_settingsPage->refresh();
    });

#ifdef QT_DEBUG
    // Debug button – visible by default in debug builds, toggled with F11
    m_debugNavBtn = makeNavBtn(tr("Debug"), QStringLiteral(":/assets/bug.svg"));
    connect(m_debugNavBtn, &QToolButton::clicked, this, [this]()
    {
        showPage(Page::Debug);
    });
#endif
}

void MainWindow::showPage(Page page)
{
#ifdef QT_DEBUG
    // Track where we came from so leaving Debug can return to the right page.
    // A sign-in preview belongs to the Debug page, so leaving it keeps the
    // page Debug was opened from (and its sidebar).
    if (page == Page::Debug)
    {
        const Page current = static_cast<Page>(m_stack->currentIndex());
        if (current != Page::Debug && m_signinPreview == false)
        {
            m_preDebugPage = current;
        }
    }
    if (page != Page::Login)
    {
        m_signinPreview = false;
    }

    const bool onLoginPage = (page == Page::Login)
        || (page == Page::Debug && m_preDebugPage == Page::Login);
#else
    const bool onLoginPage = (page == Page::Login);
#endif

    m_sidebar->setVisible(onLoginPage == false);
    m_sidebarDivider->setVisible(onLoginPage == false);

    m_stack->setCurrentIndex(std::to_underlying(page));
    placeCountriesPage();

    m_logoBtn->setChecked(page == Page::Vpn);
    m_countriesNavBtn->setChecked(page == Page::Countries);
    m_accountNavBtn->setChecked(page == Page::Account);
    m_settingsNavBtn->setChecked(page == Page::Settings);
#ifdef QT_DEBUG
    if (m_debugNavBtn != nullptr)
    {
        m_debugNavBtn->setChecked(page == Page::Debug);
    }
    if (m_loginDebugBtn != nullptr)
    {
        m_loginDebugBtn->setVisible(onLoginPage);
        if (onLoginPage)
        {
            // Rewire: on login page → go to debug; on debug page (from login) → go back to login.
            disconnect(m_loginDebugBtn, &QToolButton::clicked, nullptr, nullptr);
            if (page == Page::Login)
            {
                connect(m_loginDebugBtn, &QToolButton::clicked, this, [this]() { showPage(Page::Debug); });
            }
            else // Page::Debug with m_preDebugPage == Page::Login
            {
                connect(m_loginDebugBtn, &QToolButton::clicked, this, [this]() { showPage(Page::Login); });
            }
            repositionLoginDebugBtn();
            m_loginDebugBtn->raise();
        }
    }
    // Apply left padding on the debug page so content stays clear of the floating debug button.
    const bool debugFromLogin = (page == Page::Debug && m_preDebugPage == Page::Login);
    m_debugPage->setLeftPadding(debugFromLogin ? (SIDEBAR_MARGIN + NAV_BTN_SIZE + SIDEBAR_MARGIN) : 0);
#endif
}

void MainWindow::showNotInstalled()
{
    m_sidebar->setEnabled(false);
    showPage(Page::NotInstalled);
}

void MainWindow::showCliNotResponding()
{
    // Navigation stays locked until the retry's login check succeeds, the
    // same as on the other pages shown before startup completes.
    m_sidebar->setEnabled(false);
    showPage(Page::CliNotResponding);
}

#ifdef QT_DEBUG
void MainWindow::repositionLoginDebugBtn()
{
    if (m_loginDebugBtn == nullptr)
        return;
    const int margin = SIDEBAR_MARGIN;
    m_loginDebugBtn->move(margin, height() - m_loginDebugBtn->height() - margin);
}
#endif

void MainWindow::setNavActive(const QToolButton* btn)
{
    for (QToolButton* b : {m_logoBtn, m_countriesNavBtn, m_accountNavBtn, m_settingsNavBtn})
    {
        b->setChecked(b == btn);
    }
#ifdef QT_DEBUG
    if (m_debugNavBtn != nullptr)
    {
        m_debugNavBtn->setChecked(m_debugNavBtn == btn);
    }
#endif
}

void MainWindow::startupCheck() const
{
    m_manager->checkInstalled();
}

void MainWindow::checkForUpdates()
{
    if (AppConfig::instance().checkForUpdates() == false)
        return;

    DBG_APP(QStringLiteral("Checking for updates..."));
    QNetworkRequest request(QUrl(QString::fromLatin1(UPDATE_VERSION_URL)));
    request.setTransferTimeout(UPDATE_CHECK_TIMEOUT_MS);

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
        {
            DBG_APP(QStringLiteral("Update check failed: ") + reply->errorString());
            return;
        }

        const QJsonObject remoteObj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString remoteVersion = remoteObj.value(QStringLiteral("app_version")).toString();
        if (remoteVersion.isEmpty())
            return;

        QString localVersion;
        QFile vf(QStringLiteral(":/version.json"));
        if (vf.open(QIODevice::ReadOnly))
        {
            localVersion = QJsonDocument::fromJson(vf.readAll())
                               .object().value(QStringLiteral("app_version")).toString();
        }
        if (localVersion.isEmpty())
            return;

        const QVersionNumber remote = QVersionNumber::fromString(remoteVersion);
        const QVersionNumber local  = QVersionNumber::fromString(localVersion);

        if (remote > local)
        {
            DBG_APP(QStringLiteral("Update available: v") + localVersion
                    + QStringLiteral(" → v") + remoteVersion);
            UpdateAvailableDialog* dlg = new UpdateAvailableDialog(localVersion, remoteVersion, this);
            dlg->setModal(true);
            dlg->show();
        }
        else
        {
            DBG_APP(QStringLiteral("Up to date (v") + localVersion + QStringLiteral(")"));
        }
    });
}

void MainWindow::refreshIcons()
{
    // Logo button: render with original SVG colors (no tinting) - it's a branded icon.
    setWindowIcon(svgNavIcon(QStringLiteral(":/assets/proton-vpn-sign.svg"), {WINDOW_ICON_SIZE, WINDOW_ICON_SIZE}, false));
    m_logoBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/proton-vpn-sign.svg"), {LOGO_BTN_ICON_SIZE, LOGO_BTN_ICON_SIZE}, false));
    // Nav icons: monochrome utility icons - tint for legibility.
    m_countriesNavBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/server-smart-routing.svg"), {NAV_ICON_SIZE, NAV_ICON_SIZE}));
    m_accountNavBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/person-lines-fill.svg"), {NAV_ICON_SIZE, NAV_ICON_SIZE}));
    m_settingsNavBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/gear.svg"), {NAV_ICON_SIZE, NAV_ICON_SIZE}));
#ifdef QT_DEBUG
    if (m_debugNavBtn != nullptr)
    {
        m_debugNavBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/bug.svg"), {NAV_ICON_SIZE, NAV_ICON_SIZE}));
    }
    if (m_loginDebugBtn != nullptr)
    {
        m_loginDebugBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/bug.svg"), {NAV_ICON_SIZE, NAV_ICON_SIZE}));
    }
#endif
}

void MainWindow::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        refreshIcons();
    }
}

// Asks the user what to do when quitting with an active VPN connection.
// Returns true when the caller should go ahead and quit.  Shared by the tray's
// Quit action and by closeEvent() on desktops with no tray, where closing the
// window really does exit the app.
bool MainWindow::confirmQuit()
{
    const VpnState state = m_manager->currentState();
    if (state != VpnState::Connected && state != VpnState::Connecting)
        return true;

    QuitDialog dlg(m_vpnPage->isPortForwardingActive(), this);
    const int result = dlg.exec();
    if (result == QDialog::Rejected) return false;

    if (result == QuitDialog::DisconnectResult)
    {
        VpnManager::disconnectVpnSync(); // blocks until protonvpn disconnect finishes
    }
    return true;
}

// Closing the window (the titlebar X) hides to the tray when "Close to Tray"
// is on (the default), leaving the tray icon's "Quit" action as the way out.
// With the setting off - or on a desktop with no tray to hide to, where
// hiding would strand the user - the close really does quit, so it gets the
// same confirmation the tray's Quit action shows.
void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_trayIcon != nullptr && AppConfig::instance().closeToTray() == true)
    {
        event->ignore();
        hide();
        return;
    }

    if (confirmQuit() == false)
    {
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
}

void MainWindow::applySplitView(const bool split)
{
    if (m_splitView == split) return;
    m_splitView = split;

    m_vpnPage->setMinimumWidth(split == true ? SPLIT_VPN_MIN_WIDTH : 0);
    m_vpnPage->setSplitView(split);
    if (split == false)
    {
        m_splitSaveTimer->stop();
    }
    placeCountriesPage();
}

void MainWindow::placeCountriesPage()
{
    // Beside the VPN page in the split view, except while the Countries page
    // is open on its own from the nav; in its own stack slot otherwise.
    const bool beside = m_splitView == true
                     && m_stack->currentIndex() != std::to_underlying(Page::Countries);
    const bool inSplitter = m_countriesPage->parentWidget() == m_homeSplitter;
    if (beside == inSplitter) return;

    QLayout* hostLayout = m_countriesHost->layout();
    m_countriesPage->setSplitView(beside);
    if (beside == true)
    {
        hostLayout->removeWidget(m_countriesPage);
        m_countriesPage->setMinimumWidth(SPLIT_COUNTRIES_MIN_WIDTH);
        m_homeSplitter->insertWidget(0, m_countriesPage);
        m_countriesPage->show();
        limitCountriesWidth();
        applySplitRatio();
    }
    else
    {
        m_countriesPage->setMinimumWidth(0);
        // Reparenting takes it out of the splitter.
        m_countriesPage->setParent(m_countriesHost);
        hostLayout->addWidget(m_countriesPage);
        m_countriesPage->show();
        limitCountriesWidth();
    }
}

void MainWindow::applySplitRatio()
{
    if (m_splitView == false) return;
    // The stack, not the splitter: when the VPN page is not the current page
    // the splitter is hidden and its width is stale.
    const int total = m_stack->width() - m_homeSplitter->handleWidth();
    const int countriesW = qRound(total * AppConfig::instance().splitViewCountriesRatio());
    m_homeSplitter->setSizes({countriesW, total - countriesW});
}

void MainWindow::updateSplitView()
{
    applySplitView(width() >= SPLIT_VIEW_MIN_WINDOW_WIDTH && AppConfig::instance().showCountriesSidebar() == true);
    limitCountriesWidth();
}

void MainWindow::limitCountriesWidth()
{
    // A third of the width at most, dragging the divider included: a
    // splitter keeps to its widgets' maximum widths.
    const bool beside = m_countriesPage->parentWidget() == m_homeSplitter;
    const int limit = beside == true
        ? static_cast<int>((m_stack->width() - m_homeSplitter->handleWidth()) * AppConfig::SPLIT_RATIO_MAX)
        : QWIDGETSIZE_MAX;
    m_countriesPage->setMaximumWidth(limit);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateSplitView();
#ifdef QT_DEBUG
    repositionLoginDebugBtn();
#endif
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
#ifdef QT_DEBUG
    if (event->key() == Qt::Key_F11 && m_debugNavBtn != nullptr)
    {
        m_debugNavBtn->setVisible(m_debugNavBtn->isVisible() == false);
        // If the debug page is currently shown and we just hid the button,
        // navigate away to avoid being stuck on a visually orphaned page.
        if (m_debugNavBtn->isVisible() == false &&
            m_stack->currentIndex() == std::to_underlying(Page::Debug))
        {
            showPage(m_preDebugPage);
        }
        event->accept();
        return;
    }
#endif
    QWidget::keyPressEvent(event);
}

void MainWindow::sendNotification(const QString& title, const QString& message) const
{
    if (AppConfig::instance().notifications() == false)
        return;
    if (m_trayIcon == nullptr)
        return; // notifications are delivered through the tray icon

    // Render the ProtonVPN sign SVG into a pixmap to use as the notification icon.
    const QPixmap iconPix = GeoUtils::svgPixmap(QStringLiteral(":/assets/proton-vpn-sign.svg"),
                                                NOTIFICATION_ICON_SIZE);

    m_trayIcon->showMessage(title, message, QIcon(iconPix), NOTIFICATION_DURATION_MS);
}

void MainWindow::updateTrayIcon(VpnState state)
{
    // Choose asset based on state
    QString asset;
    switch (state)
    {
        case VpnState::Connected:
            asset = QStringLiteral(":/assets/state-connected.svg");
            break;

        case VpnState::Connecting:
        case VpnState::Disconnecting:
            asset = QStringLiteral(":/assets/state-connecting.svg");
            break;

        case VpnState::Error:
            asset = QStringLiteral(":/assets/state-error.svg");
            break;

        default:
            asset = QStringLiteral(":/assets/state-disconnected.svg");
            break;
    }

    if (m_trayIcon != nullptr)
    {
        m_trayIcon->setIcon(QIcon(GeoUtils::svgPixmap(asset, TRAY_ICON_SIZE)));
        updateTrayTooltipAndAction(state);
    }

    notifyStateTransition(state);
}

// Keeps the tray tooltip and the Connect/Disconnect action in step with the
// current state.  Only called when a tray icon exists.
void MainWindow::updateTrayTooltipAndAction(const VpnState state) const
{
    switch (state)
    {
        case VpnState::Connected:
            m_trayIcon->setToolTip(tr("ProtonVPN \u2013 Connected"));
            m_trayConnectAction->setText(tr("Disconnect"));
            m_trayConnectAction->setEnabled(true);
            break;

        case VpnState::Connecting:
            m_trayIcon->setToolTip(tr("ProtonVPN \u2013 Connecting\u2026"));
            m_trayConnectAction->setText(tr("Connecting\u2026"));
            m_trayConnectAction->setEnabled(false);
            break;

        case VpnState::Disconnecting:
            m_trayIcon->setToolTip(tr("ProtonVPN \u2013 Disconnecting\u2026"));
            m_trayConnectAction->setText(tr("Disconnecting\u2026"));
            m_trayConnectAction->setEnabled(false);
            break;

        case VpnState::Error:
            m_trayIcon->setToolTip(tr("ProtonVPN \u2013 Error"));
            m_trayConnectAction->setText(tr("Connect"));
            m_trayConnectAction->setEnabled(true);
            break;

        case VpnState::Disconnected:
            m_trayIcon->setToolTip(tr("ProtonVPN \u2013 Disconnected"));
            m_trayConnectAction->setText(tr("Connect"));
            m_trayConnectAction->setEnabled(true);
            break;

        default: // Unknown - still checking
            m_trayIcon->setToolTip(tr("ProtonVPN \u2013 Checking\u2026"));
            m_trayConnectAction->setText(tr("Connect"));
            m_trayConnectAction->setEnabled(false);
            break;
    }
}

// Sends a desktop notification on meaningful state transitions, skipping
// repeats of a state that was already announced.
void MainWindow::notifyStateTransition(const VpnState state)
{
    if (state != m_lastNotifiedState)
    {
        switch (state)
        {
            case VpnState::Connecting:
                sendNotification(tr("ProtonVPN \u2013 Connecting"),
                                 tr("Establishing a secure VPN connection\u2026"));
                break;

            case VpnState::Disconnecting:
                sendNotification(tr("ProtonVPN \u2013 Disconnecting"),
                                 tr("Closing the VPN connection\u2026"));
                break;

            case VpnState::Connected:
                sendNotification(tr("ProtonVPN \u2013 Connected"),
                                 tr("You are now protected by ProtonVPN."));
                break;

            case VpnState::Disconnected:
                // Only notify on disconnect if we were previously connected/connecting
                if (m_lastNotifiedState == VpnState::Connected ||
                    m_lastNotifiedState == VpnState::Disconnecting)
                {
                    sendNotification(tr("ProtonVPN \u2013 Disconnected"),
                                     tr("The VPN connection has been closed."));
                }
                break;

            default:
                break;
        }
        m_lastNotifiedState = state;
    }
}

void MainWindow::maybeShowWhatsNew()
{
    if (m_whatsNewShown)
        return;

    m_whatsNewShown = true;

    // Read the current app version from the embedded version.json resource.
    QString currentVersion;
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly))
    {
        const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
        vf.close();
        currentVersion = obj.value(QStringLiteral("app_version")).toString();
    }

    if (currentVersion.isEmpty())
        return;

    const QString lastSeen = AppConfig::instance().lastSeenVersion();

    // Show the dialog if this is the first launch or a version change was detected.
    if (lastSeen != currentVersion)
    {
        // Update the stored version immediately so repeated crashes don't keep
        // showing the dialog on every launch.
        AppConfig::instance().setLastSeenVersion(currentVersion);

        auto* dlg = new WhatsNewDialog(currentVersion, this);
        dlg->setModal(true);
        dlg->show();
    }
}
