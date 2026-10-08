#include "settingsPage.h"
#include "../appConfig.h"
#include "../motionPreference.h"
#include "../cli/cliSettings.h"
#include "../connectionHistory.h"
#include "../debug.h"
#include "../favoritesManager.h"
#include "../fileLogger.h"
#include "../geoUtils.h"
#include "../themeManager.h"
#include "../uiHelpers.h"
#include "../cli/platformUtils.h"
#include "../quickSettingIds.h"
#include "../dialogs/natpmpcDialog.h"
#include "../dialogs/reconnectDialog.h"
#include "../widgets/cliSettingControls.h"
#include "../widgets/globeAnimationCombo.h"
#include "../widgets/numberSpinner.h"
#include "../widgets/plusDivider.h"
#include "../widgets/quickSettingsMenu.h"
#include "../widgets/styledComboBox.h"
#include "../widgets/toastNotification.h"
#include "../widgets/toggleWithStatus.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QSignalBlocker>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
// ReSharper disable once CppUnusedIncludeDirective
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>
#include <QMessageBox>
#include <QMouseEvent>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTextBrowser>
#include <QVersionNumber>
#include <QVBoxLayout>
#include <optional>
#include <tuple>

namespace
{
// makeTextCol
constexpr int TEXT_COL_SPACING              = 2;
constexpr int DESC_FONT_MIN_SIZE            = 7;
constexpr int DESC_FONT_REDUCTION           = 1;

// makeToggleRow / makeComboRow
constexpr int SETTING_ROW_H_MARGIN          = 16;
constexpr int SETTING_ROW_V_MARGIN          = 12;
constexpr int SETTING_ROW_SPACING           = 16;
constexpr int COMBO_MIN_WIDTH               = 160;
constexpr int DNS_ADDR_BOT_MARGIN           = 12;

// constructor outer layout
constexpr int OUTER_LAYOUT_MARGIN           = 16;
constexpr int OUTER_LAYOUT_SPACING          = 12;
constexpr int PAGE_LAYOUT_TOP_MARGIN        = 8;
constexpr int PAGE_LAYOUT_SPACING           = 8;
constexpr int APP_CONTENT_BOT_MARGIN        = 8;
constexpr int SECTION_HEADER_H_MARGIN       = 4;
constexpr int SECTION_HEADER_LEFT_MARGIN    = 12;
constexpr int SECTION_HEADER_TOP_MARGIN     = 16;
constexpr int SECTION_HEADER_BOT_MARGIN     = 4;
constexpr int AUTOSTART_SUB_LEFT_MARGIN     = 32;
constexpr int SERVER_COMBO_LEFT_MARGIN      = 48;
constexpr int SERVER_COMBO_V_MARGIN         = 4;
constexpr int SERVER_COMBO_MIN_WIDTH        = 180;
constexpr int WRAPPER_TOP_MARGIN            = 20;
constexpr int REFRESH_BTN_HEIGHT            = 30;
constexpr int SPINNER_INTERVAL_MS           = 200;

// kill switch sub-panel
constexpr int KS_OPTION_V_MARGIN            = 10;
constexpr int KS_OPTION_H_SPACING          = 10;
constexpr int KS_SUBPANEL_H_MARGIN          = 16;
constexpr int KS_SUBPANEL_BOT_MARGIN        = 12;
constexpr int KS_TEXT_COL_SPACING           = 2;

// port forwarding row
constexpr int PORT_ROW_H_MARGIN             = 16;
constexpr int PORT_ROW_BOT_MARGIN           = 12;
constexpr int PORT_ROW_SPACING              = 8;
constexpr int PORT_COPY_ICON_SIZE           = 16;
constexpr int PORT_COPY_BTN_W              = 34;
constexpr int PORT_COPY_BTN_H              = 30;
constexpr int PORT_FONT_SIZE_BOOST          = 1;

// theme combo indices
constexpr int THEME_IDX_SYSTEM              = 0;
constexpr int THEME_IDX_DARK                = 1;
constexpr int THEME_IDX_LIGHT               = 2;

constexpr int RECENT_CONNECTIONS_MAX = 20;

constexpr int DNS_APPLY_BTN_HEIGHT          = 28;

// updatePlusSectionState
constexpr double PLUS_SECTION_DISABLED_OPACITY = 0.45;

// eventFilter refresh button overlay
constexpr int REFRESH_OVERLAY_MARGIN        = 8;
constexpr int REFRESH_OVERLAY_H             = 30;


#ifdef QT_DEBUG
constexpr bool DRY_RUN_MODE = true;
#else
constexpr bool DRY_RUN_MODE = false;
#endif

QWidget* makeTextCol(QWidget* parent, const QString& label, const QString& desc)
{
    QWidget* w = new QWidget(parent);
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QVBoxLayout* col = new QVBoxLayout(w);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(TEXT_COL_SPACING);
    QLabel* nameL = new QLabel(label, w);
    nameL->setObjectName(QStringLiteral("infoKey"));
    col->addWidget(nameL);
    if (desc.isEmpty() == false)
    {
        QLabel* descL = new QLabel(desc, w);
        descL->setObjectName(QStringLiteral("settingsDesc"));
        descL->setWordWrap(true);
        descL->setOpenExternalLinks(true);
        descL->setTextInteractionFlags(Qt::TextBrowserInteraction);
        QFont f = descL->font();
        f.setPointSize(qMax(f.pointSize() - DESC_FONT_REDUCTION, DESC_FONT_MIN_SIZE));
        descL->setFont(f);
        col->addWidget(descL);
    }
    return w;
}

// A setting row: its name and description on the left, and on the right
// whatever control the caller adds to the returned layout.
std::pair<QWidget*, QHBoxLayout*> makeSettingRow(QWidget* parent, const QString& label, const QString& desc)
{
    QWidget* row = new QWidget(parent);
    QHBoxLayout* rl = new QHBoxLayout(row);
    rl->setContentsMargins(SETTING_ROW_H_MARGIN, SETTING_ROW_V_MARGIN,
                           SETTING_ROW_H_MARGIN, SETTING_ROW_V_MARGIN);
    rl->setSpacing(SETTING_ROW_SPACING);
    rl->addWidget(makeTextCol(row, label, desc), 1);
    return {row, rl};
}
} // namespace

// ============================================================
// SettingsPage helpers
// ============================================================

void SettingsPage::addDivider(QVBoxLayout* layout, QWidget* parent)
{
    QFrame* div = new QFrame(parent);
    div->setFrameShape(QFrame::HLine);
    div->setObjectName(QStringLiteral("divider"));
    layout->addWidget(div);
}

void SettingsPage::maybeWarnReconnect(const QString& cliOutput)
{
    // The CLI emits phrases like "please establish a new VPN connection for
    // changes to take effect" when a reconnect is required.
    // Only warn when we are actually connected so the message is relevant.
    const bool needsReconnect =
        cliOutput.contains(QStringLiteral("new VPN connection"), Qt::CaseInsensitive) ||
        cliOutput.contains(QStringLiteral("establish a new"), Qt::CaseInsensitive);

    if (needsReconnect == false) return;
    if (m_manager->currentState() != VpnState::Connected) return;

    QMessageBox mb(this);
    mb.setWindowTitle(tr("Reconnect Required"));
    mb.setIcon(QMessageBox::Information);
    mb.setText(tr("This setting change will only take effect after reconnecting to the VPN."));
    mb.setStandardButtons(QMessageBox::Ok);
    mb.exec();
}

void SettingsPage::showReconnectDialog(const QString& settingLabel,
                                       const std::function<void()>& onAccept)
{
    if (ReconnectDialog::confirm(settingLabel, this) == true)
    {
        onAccept();
    }
}

void SettingsPage::addQuickSetting(const QuickSettingSource& source)
{
    m_quickSettingSources.append(source);

    // Right-clicking anywhere on the row brings up the menu, its labels
    // included: they would otherwise offer to copy their text and links.
    source.row->setContextMenuPolicy(Qt::CustomContextMenu);
    const QList<QLabel*> labels = source.row->findChildren<QLabel*>();
    for (QLabel* label : labels)
    {
        label->setContextMenuPolicy(Qt::NoContextMenu);
    }
    connect(source.row, &QWidget::customContextMenuRequested, this,
            [this, id = source.id, row = source.row](const QPoint& pos)
    {
        if (QuickSettingsMenu::exec(id, row->mapToGlobal(pos), this) == true)
        {
            emit quickSettingsChanged();
        }
    });
}

QWidget* SettingsPage::makeToggleRow(QWidget* parent, const QString& label,
                                     const QString& desc, const QString& cliKey,
                                     const QString& onValue, const bool requiresReconnect)
{
    auto [row, rl] = makeSettingRow(parent, label, desc);
    ToggleWithStatus* toggle = new ToggleWithStatus(row);
    rl->addWidget(toggle, 0);
    m_cliControls->addToggle(toggle, label, cliKey, onValue, requiresReconnect);
    return row;
}

QWidget* SettingsPage::makeComboRow(QWidget* parent, const QString& label,
                                    const QString& desc, const QString& cliKey,
                                    const QStringList& labels, const QStringList& cliValues,
                                    const bool requiresReconnect)
{
    auto [row, rl] = makeSettingRow(parent, label, desc);
    StyledComboBox* combo = new StyledComboBox(row);
    combo->addItems(labels);
    combo->setMinimumWidth(COMBO_MIN_WIDTH);
    rl->addWidget(combo, 0);
    m_cliControls->addCombo(combo, label, cliKey, cliValues, requiresReconnect);
    return row;
}

// ============================================================
// SettingsPage constructor
// ============================================================

// Scroll area + "infoCard" container for a settings tab.
// static
std::pair<QWidget*, QVBoxLayout*> SettingsPage::makeTabCard(QWidget* tabPage)
{
    QWidget* card = new QWidget();
    card->setObjectName(QStringLiteral("infoCard"));
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);
    addTabScrollArea(tabPage, card);
    return {card, cardLayout};
}

// static
void SettingsPage::addTabScrollArea(QWidget* tabPage, QWidget* content)
{
    QScrollArea* scroll = new QScrollArea(tabPage);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    // The viewport's own background is handled by the global
    // "QScrollArea > QWidget" QSS rule, not a local setStyleSheet() call
    // here: a *local* stylesheet on an ancestor silently breaks
    // background-color painting for ID-selector-styled descendants
    // further down the tree (e.g. #infoCard), even though borders still
    // render fine. Confirmed by isolated testing; cost real time to find.
    QVBoxLayout* pageLayout = new QVBoxLayout(tabPage);
    pageLayout->setContentsMargins(0, PAGE_LAYOUT_TOP_MARGIN, 0, 0);
    pageLayout->setSpacing(PAGE_LAYOUT_SPACING);
    pageLayout->addWidget(scroll, 1);
    scroll->setWidget(content);
}

// static
std::pair<QWidget*, QVBoxLayout*> SettingsPage::makeSectionedTab(QWidget* tabPage)
{
    QWidget* content = new QWidget();
    QVBoxLayout* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, APP_CONTENT_BOT_MARGIN);
    contentLayout->setSpacing(PAGE_LAYOUT_SPACING);
    addTabScrollArea(tabPage, content);
    return {content, contentLayout};
}

// static
void SettingsPage::addSectionHeader(QWidget* parent, QVBoxLayout* layout, const QString& title)
{
    if (layout->count() > 0)
    {
        QFrame* sep = new QFrame(parent);
        sep->setFrameShape(QFrame::HLine);
        sep->setObjectName(QStringLiteral("appSectionDivider"));
        layout->addWidget(sep);
    }

    QWidget* w = new QWidget(parent);
    QHBoxLayout* hl = new QHBoxLayout(w);
    hl->setContentsMargins(SECTION_HEADER_LEFT_MARGIN, SECTION_HEADER_TOP_MARGIN,
                           SECTION_HEADER_H_MARGIN, SECTION_HEADER_BOT_MARGIN);
    QLabel* lbl = new QLabel(title.toUpper(), w);
    lbl->setObjectName(QStringLiteral("appSectionHeader"));
    hl->addWidget(lbl);
    layout->addWidget(w);
}

// static
std::pair<QWidget*, QVBoxLayout*> SettingsPage::addSectionCard(QWidget* parent, QVBoxLayout* layout)
{
    QWidget* card = new QWidget(parent);
    card->setObjectName(QStringLiteral("infoCard"));
    QVBoxLayout* cl = new QVBoxLayout(card);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
    layout->addWidget(card);
    return {card, cl};
}

// App tab: startup, notifications, history, favorites, logging, about.
void SettingsPage::buildAppTab(QTabWidget* tabs)
{
    // ============================================================
    // TAB 1 – App
    // ============================================================
    QWidget* appTab = new QWidget();
    tabs->addTab(appTab, tr("App"));

    {
        using Section = QuickSettingSource::Section;
        using Kind = QuickSettingSource::Kind;
        auto [appContent, appContentLayout] = makeSectionedTab(appTab);

        auto addHeader = [&](const QString& title)
        {
            addSectionHeader(appContent, appContentLayout, title);
        };

        auto makeAppCard = [&]() -> std::pair<QWidget*, QVBoxLayout*>
        {
            return addSectionCard(appContent, appContentLayout);
        };

        //  Section: Startup
        addHeader(tr("Startup"));
        auto [startupCard, startupLayout] = makeAppCard();

        bool startupFirst = true;
        auto addStartup = [&](QWidget* w)
        {
            if (startupFirst == false)
            {
                addDivider(startupLayout, startupCard);
            }
            startupFirst = false;
            startupLayout->addWidget(w);
        };

        {
            QHBoxLayout* rl = nullptr;
            std::tie(m_autoStartRow, rl) = makeSettingRow(startupCard,
                                                          tr("Launch on Startup"),
                                                          tr("Start Proton VPN automatically when you log in."));
            m_autoStartToggle = new ToggleWithStatus(m_autoStartRow);
            m_autoStartToggle->setOn(autoStartEnabled(), false);
            connect(m_autoStartToggle, &ToggleWithStatus::toggled, this, [this](const bool on)
            {
                QString err;
                if (setAutoStart(on, err) == false)
                {
                    m_autoStartToggle->setOn(on == false, false);
                    QMessageBox::warning(this,
                                         tr("Auto-start Error"),
                                         tr("Failed to %1 auto-start:\n%2")
                                         .arg(on == true ? tr("enable") : tr("disable"), err));
                }
                else
                {
                    updateAutoConnectRowVisibility();
                }
            });
            rl->addWidget(m_autoStartToggle);
            addStartup(m_autoStartRow);
            addQuickSetting({.id = QuickSettingIds::LAUNCH_ON_STARTUP, .label = tr("Launch on Startup"),
                             .section = Section::App, .row = m_autoStartRow, .control = m_autoStartToggle});

            // Sub-row: indented auto-connect (no divider – belongs to the row above)
            m_autoConnectRow = new QWidget(startupCard);
            QHBoxLayout* acRl = new QHBoxLayout(m_autoConnectRow);
            acRl->setContentsMargins(AUTOSTART_SUB_LEFT_MARGIN, PAGE_LAYOUT_SPACING,
                                     SETTING_ROW_H_MARGIN, SETTING_ROW_V_MARGIN);
            acRl->setSpacing(SETTING_ROW_SPACING);
            acRl->addWidget(makeTextCol(m_autoConnectRow,
                                        tr("Auto-connect on Startup"),
                                        tr("Automatically connect to the VPN when the app starts.")), 1);
            m_autoConnectToggle = new ToggleWithStatus(m_autoConnectRow);
            m_autoConnectToggle->setOn(AppConfig::instance().autoConnect(), false);
            connect(m_autoConnectToggle, &ToggleWithStatus::toggled, this, [](const bool on)
            {
                AppConfig::instance().setAutoConnect(on);
            });
            acRl->addWidget(m_autoConnectToggle);
            startupLayout->addWidget(m_autoConnectRow); // direct add – no divider above it
            addQuickSetting({.id = QuickSettingIds::AUTO_CONNECT, .label = tr("Auto-connect on Startup"),
                             .section = Section::App, .row = m_autoConnectRow, .control = m_autoConnectToggle});
            updateAutoConnectRowVisibility();

            // Sub-sub-row: indented server dropdown (no divider – belongs to auto-connect)
            m_autoConnectServerRow = new QWidget(startupCard);
            QHBoxLayout* srvRl = new QHBoxLayout(m_autoConnectServerRow);
            srvRl->setContentsMargins(SERVER_COMBO_LEFT_MARGIN, SERVER_COMBO_V_MARGIN,
                                      SETTING_ROW_H_MARGIN, SETTING_ROW_V_MARGIN);
            srvRl->setSpacing(SETTING_ROW_SPACING);
            srvRl->addWidget(makeTextCol(m_autoConnectServerRow,
                                         tr("Server"),
                                         tr("Choose which server to connect to on startup.")), 1);
            m_autoConnectServerCombo = new StyledComboBox(m_autoConnectServerRow);
            m_autoConnectServerCombo->setMinimumWidth(SERVER_COMBO_MIN_WIDTH);
            populateAutoConnectServerCombo();
            connect(m_autoConnectServerCombo, &QComboBox::currentIndexChanged, this, [this](const int idx)
            {
                const QString val = m_autoConnectServerCombo->itemData(idx).toString();
                AppConfig::instance().setAutoConnectServer(val);
            });
            // Keep the combo up-to-date when favorites change.
            connect(&FavoritesManager::instance(), &FavoritesManager::changed, this, [this]()
            {
                populateAutoConnectServerCombo();
                updateAutoConnectServerRow();
            });
            // Show/hide when auto-connect toggle changes.
            connect(m_autoConnectToggle, &ToggleWithStatus::toggled, this, [this](bool)
            {
                updateAutoConnectServerRow();
            });
            srvRl->addWidget(m_autoConnectServerCombo);
            startupLayout->addWidget(m_autoConnectServerRow); // direct add – no divider above it
            addQuickSetting({.id = QuickSettingIds::AUTO_CONNECT_SERVER, .label = tr("Auto-connect Server"),
                             .section = Section::App, .kind = Kind::Combo, .row = m_autoConnectServerRow,
                             .control = m_autoConnectServerCombo});
            updateAutoConnectServerRow();
        }

        // Start Hidden
        {
            auto [row, rl] = makeSettingRow(startupCard,
                                            tr("Start Hidden"),
                                            tr("Launch the app in the background without opening a "
                                               "window. Access it anytime via the system tray icon."));
            ToggleWithStatus* toggle = new ToggleWithStatus(row);
            toggle->setOn(AppConfig::instance().startHidden(), false);
            connect(toggle, &ToggleWithStatus::toggled, this, [](const bool on)
            {
                AppConfig::instance().setStartHidden(on);
            });
            rl->addWidget(toggle);
            addStartup(row);
            addQuickSetting({.id = QuickSettingIds::START_HIDDEN, .label = tr("Start Hidden"),
                             .section = Section::App, .row = row, .control = toggle});
        }

        //  Section: Window
        addHeader(tr("Window"));
        auto [windowCard, windowLayout] = makeAppCard();

        {
            auto [row, rl] = makeSettingRow(windowCard,
                                            tr("Close to Tray"),
                                            tr("Closing the window keeps ProtonVPN running in the "
                                               "system tray. Turn this off to quit the app when the "
                                               "window is closed."));
            ToggleWithStatus* closeToTrayToggle = new ToggleWithStatus(row);
            closeToTrayToggle->setOn(AppConfig::instance().closeToTray(), false);
            connect(closeToTrayToggle, &ToggleWithStatus::toggled, this, [](const bool on)
            {
                AppConfig::instance().setCloseToTray(on);
            });
            rl->addWidget(closeToTrayToggle);

            // With no system tray there is nowhere to close to, so the window
            // always quits and the choice would be a lie.
            if (QSystemTrayIcon::isSystemTrayAvailable() == false)
            {
                row->setEnabled(false);
                row->setToolTip(tr("Your desktop does not provide a system tray, so closing "
                                   "the window always quits the app."));
            }

            windowLayout->addWidget(row);
            addQuickSetting({.id = QuickSettingIds::CLOSE_TO_TRAY, .label = tr("Close to Tray"),
                             .section = Section::App, .row = row, .control = closeToTrayToggle});
        }

        //  Section: Notifications
        addHeader(tr("Notifications"));
        auto [notifCard, notifLayout] = makeAppCard();
        (void)notifLayout;

        {
            auto [row, rl] = makeSettingRow(notifCard,
                                            tr("Desktop Notifications"),
                                            tr("Show a system notification when the VPN is connecting, "
                                               "connected, disconnecting, or disconnected."));
            m_notificationsToggle = new ToggleWithStatus(row);
            m_notificationsToggle->setOn(AppConfig::instance().notifications(), false);
            connect(m_notificationsToggle, &ToggleWithStatus::toggled, this, [](const bool on)
            {
                AppConfig::instance().setNotifications(on);
            });
            rl->addWidget(m_notificationsToggle);
            notifLayout->addWidget(row);
            addQuickSetting({.id = QuickSettingIds::NOTIFICATIONS, .label = tr("Desktop Notifications"),
                             .section = Section::App, .row = row, .control = m_notificationsToggle});
        }

        {
            addDivider(notifLayout, notifCard);
            auto [row, rl] = makeSettingRow(notifCard,
                                            tr("Check for Updates on Startup"),
                                            tr("Automatically check for a newer version of the app "
                                               "each time it starts."));
            ToggleWithStatus* updateToggle = new ToggleWithStatus(row);
            updateToggle->setOn(AppConfig::instance().checkForUpdates(), false);
            connect(updateToggle, &ToggleWithStatus::toggled, this, [](const bool on)
            {
                AppConfig::instance().setCheckForUpdates(on);
            });
            rl->addWidget(updateToggle);
            notifLayout->addWidget(row);
            addQuickSetting({.id = QuickSettingIds::CHECK_FOR_UPDATES, .label = tr("Check for Updates"),
                             .section = Section::App, .row = row, .control = updateToggle});
        }

        //  Plus Members Only divider
        m_appPlusDivider = new PlusDivider(appContent);
        appContentLayout->addWidget(m_appPlusDivider);

        //  Plus-only section container
        // m_appPlusSection wraps History + Favorites so updatePlusSectionState
        // can disable/fade the whole block for free users.
        m_appPlusSection = new QWidget(appContent);
        QVBoxLayout* appPlusSectionLayout = new QVBoxLayout(m_appPlusSection);
        appPlusSectionLayout->setContentsMargins(0, 0, 0, 0);
        appPlusSectionLayout->setSpacing(PAGE_LAYOUT_SPACING);
        appContentLayout->addWidget(m_appPlusSection);

        //  Sub-section: Connection History
        addSectionHeader(m_appPlusSection, appPlusSectionLayout, tr("Connection History"));
        auto [histCard, histLayout] = addSectionCard(m_appPlusSection, appPlusSectionLayout);

        bool histFirst = true;
        auto addHist = [&](QWidget* w)
        {
            if (histFirst == false)
            {
                addDivider(histLayout, histCard);
            }
            histFirst = false;
            histLayout->addWidget(w);
        };

        {
            auto [row, rl] = makeSettingRow(histCard,
                                            tr("Recent Connections"),
                                            tr("Number of recent VPN connections to remember and show "
                                               "on the home screen. Set to 0 to disable."));
            m_recentConnectionsSpinBox = new NumberSpinner(row);
            m_recentConnectionsSpinBox->setRange(0, RECENT_CONNECTIONS_MAX);
            m_recentConnectionsSpinBox->setValue(AppConfig::instance().recentConnectionsCount());
            connect(m_recentConnectionsSpinBox, &NumberSpinner::valueChanged, this, [](const int val)
            {
                AppConfig::instance().setRecentConnectionsCount(val);
                ConnectionHistory::instance().trimToCount(val);
            });
            rl->addWidget(m_recentConnectionsSpinBox);
            addHist(row);
            addQuickSetting({.id = QuickSettingIds::RECENT_CONNECTIONS, .label = tr("Recent Connections"),
                             .section = Section::App, .kind = Kind::Number, .row = row,
                             .control = m_recentConnectionsSpinBox, .plusOnly = true});
        }

        // Clear Recent Connections
        {
            m_clearRecentRow = new QWidget(histCard);
            QVBoxLayout* cLayout = new QVBoxLayout(m_clearRecentRow);
            cLayout->setContentsMargins(0, 0, 0, 0);
            cLayout->setSpacing(0);
            QFrame* div = new QFrame(m_clearRecentRow);
            div->setFrameShape(QFrame::HLine);
            div->setObjectName(QStringLiteral("divider"));
            cLayout->addWidget(div);
            auto [inner, rl] = makeSettingRow(m_clearRecentRow,
                                              tr("Clear Recent Connections"),
                                              tr("Remove all saved recent connection history."));
            QPushButton* clearBtn = new QPushButton(tr("Clear"), inner);
            clearBtn->setObjectName(QStringLiteral("dangerButton"));
            clearBtn->setCursor(Qt::PointingHandCursor);
            connect(clearBtn, &QPushButton::clicked, this, [this]()
            {
                ConnectionHistory::instance().clear();
                m_clearRecentRow->setVisible(false);
                emit recentConnectionsCleared();
                ToastNotification::popup(this, tr("Recent connection history cleared."));
            });
            rl->addWidget(clearBtn);
            cLayout->addWidget(inner);
            histLayout->addWidget(m_clearRecentRow);
            addQuickSetting({.id = QuickSettingIds::CLEAR_RECENT_CONNECTIONS, .label = tr("Clear Recent Connections"),
                             .section = Section::App, .kind = Kind::Button, .row = m_clearRecentRow,
                             .control = clearBtn, .plusOnly = true});
            m_clearRecentRow->setVisible(ConnectionHistory::instance().hasAnyEntries());
            connect(&ConnectionHistory::instance(), &ConnectionHistory::changed, this, [this]()
            {
                m_clearRecentRow->setVisible(ConnectionHistory::instance().hasAnyEntries());
            });
        }

        //  Sub-section: Favorites
        addSectionHeader(m_appPlusSection, appPlusSectionLayout, tr("Favorites"));
        auto [favCard, favLayout] = addSectionCard(m_appPlusSection, appPlusSectionLayout);

        bool favFirst = true;
        auto addFav = [&](QWidget* w)
        {
            if (favFirst == false)
            {
                addDivider(favLayout, favCard);
            }
            favFirst = false;
            favLayout->addWidget(w);
        };

        // Enable Favorites
        {
            auto [row, rl] = makeSettingRow(favCard,
                                            tr("Enable Favorites"),
                                            tr("Allow marking VPN locations as favorites for quick access."));
            ToggleWithStatus* toggle = new ToggleWithStatus(row);
            toggle->setOn(AppConfig::instance().favoritesEnabled(), false);
            connect(toggle, &ToggleWithStatus::toggled, this, [this](const bool on)
            {
                AppConfig::instance().setFavoritesEnabled(on);
                emit favoritesEnabledChanged(on);
                if (m_showFavoritesDropdownToggle != nullptr)
                {
                    m_showFavoritesDropdownToggle->setEnabled(on);
                    const QString tip = on == true ? QString()
                                                   : tr("Enable the Favorites system to use this setting.");
                    m_showFavoritesDropdownRow->setToolTip(tip);
                    m_showFavoritesDropdownToggle->setToolTip(tip);
                }
            });
            rl->addWidget(toggle);
            addFav(row);
            addQuickSetting({.id = QuickSettingIds::ENABLE_FAVORITES, .label = tr("Enable Favorites"),
                             .section = Section::App, .row = row, .control = toggle, .plusOnly = true});
        }

        // Clear Favorites
        {
            m_clearFavoritesRow = new QWidget(favCard);
            QVBoxLayout* cLayout = new QVBoxLayout(m_clearFavoritesRow);
            cLayout->setContentsMargins(0, 0, 0, 0);
            cLayout->setSpacing(0);
            QFrame* div = new QFrame(m_clearFavoritesRow);
            div->setFrameShape(QFrame::HLine);
            div->setObjectName(QStringLiteral("divider"));
            cLayout->addWidget(div);
            auto [inner, rl] = makeSettingRow(m_clearFavoritesRow,
                                              tr("Clear Favorites"),
                                              tr("Remove all saved favorite locations."));
            QPushButton* clearBtn = new QPushButton(tr("Clear"), inner);
            clearBtn->setObjectName(QStringLiteral("dangerButton"));
            clearBtn->setCursor(Qt::PointingHandCursor);
            connect(clearBtn, &QPushButton::clicked, this, [this]()
            {
                FavoritesManager::instance().clear();
                m_clearFavoritesRow->setVisible(false);
                emit favoritesCleared();
                ToastNotification::popup(this, tr("Favorites cleared."));
            });
            rl->addWidget(clearBtn);
            cLayout->addWidget(inner);
            favLayout->addWidget(m_clearFavoritesRow);
            addQuickSetting({.id = QuickSettingIds::CLEAR_FAVORITES, .label = tr("Clear Favorites"),
                             .section = Section::App, .kind = Kind::Button, .row = m_clearFavoritesRow,
                             .control = clearBtn, .plusOnly = true});
            m_clearFavoritesRow->setVisible(FavoritesManager::instance().hasAnyEntries());
            connect(&FavoritesManager::instance(), &FavoritesManager::changed, this, [this]()
            {
                m_clearFavoritesRow->setVisible(FavoritesManager::instance().hasAnyEntries());
            });
        }

        //  Section: Logging
        addHeader(tr("Logging"));
        auto [loggingCard, loggingLayout] = makeAppCard();

        {
            auto [row, rl] = makeSettingRow(loggingCard,
                                            tr("Write Logs to File"),
                                            tr("Save diagnostic logs to disk for troubleshooting. "
                                               "The %1 most recent log files are kept.")
                                                .arg(FileLogger::MAX_ROTATED_LOGS));
            ToggleWithStatus* logToFileToggle = new ToggleWithStatus(row);
            logToFileToggle->setOn(AppConfig::instance().logToFile(), false);
            connect(logToFileToggle, &ToggleWithStatus::toggled, this, [](const bool on)
            {
                AppConfig::instance().setLogToFile(on);
            });
            rl->addWidget(logToFileToggle);
            loggingLayout->addWidget(row);
            addQuickSetting({.id = QuickSettingIds::LOG_TO_FILE, .label = tr("Write Logs to File"),
                             .section = Section::App, .row = row, .control = logToFileToggle});
        }

        {
            auto [row, rl] = makeSettingRow(loggingCard,
                                            tr("Verbose Logging"),
                                            tr("Print extra detail to the log, such as every server and "
                                               "node a location's Properties lists. Launching with "
                                               "<code>--verbose</code> turns it on for one run."));
            ToggleWithStatus* verboseToggle = new ToggleWithStatus(row);
            verboseToggle->setOn(AppConfig::instance().verboseLogging(), false);
            connect(verboseToggle, &ToggleWithStatus::toggled, this, [](const bool on)
            {
                AppConfig::instance().setVerboseLogging(on);
            });
            rl->addWidget(verboseToggle);
            loggingLayout->addWidget(row);
            addQuickSetting({.id = QuickSettingIds::VERBOSE_LOGGING, .label = tr("Verbose Logging"),
                             .section = Section::App, .row = row, .control = verboseToggle});
        }

        //  Section: About
        addHeader(tr("About"));
        auto [aboutCard, aboutLayout] = makeAppCard();
        (void)aboutLayout;

        {
            const QPixmap arrowPm = GeoUtils::svgPixmap(
                QStringLiteral(":/assets/box-arrow-up-right.svg"), SETTING_ROW_H_MARGIN,
                SETTING_ROW_H_MARGIN,
                QColor(QStringLiteral("#9999bb")));

            QHBoxLayout* rl = nullptr;
            std::tie(m_aboutRow, rl) = makeSettingRow(aboutCard,
                                                      tr("About Proton VPN"),
                                                      tr("View app version, licenses, and credits."));
            m_aboutRow->setObjectName(QStringLiteral("appNavRow"));
            m_aboutRow->setCursor(Qt::PointingHandCursor);
            m_aboutRow->installEventFilter(this);
            QLabel* iconLbl = new QLabel(m_aboutRow);
            iconLbl->setPixmap(arrowPm);
            iconLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            rl->addWidget(iconLbl);
            aboutLayout->addWidget(m_aboutRow);
        }

        appContentLayout->addStretch();
    } // end App tab scroll area
}

// Appearance tab: theme, the VPN page's globe, and which pickers it shows.
void SettingsPage::buildAppearanceTab(QTabWidget* tabs)
{
    // ============================================================
    // TAB 2 – Appearance
    // ============================================================
    {
        using Section = QuickSettingSource::Section;
        using Kind = QuickSettingSource::Kind;
        QWidget* appearanceTab = new QWidget();
        auto [appearanceContent, appearanceContentLayout] = makeSectionedTab(appearanceTab);
        tabs->addTab(appearanceTab, tr("Appearance"));

        //  Section: General
        addSectionHeader(appearanceContent, appearanceContentLayout, tr("General"));
        auto [generalCard, generalLayout] = addSectionCard(appearanceContent, appearanceContentLayout);

        //  Theme
        {
            auto [row, rl] = makeSettingRow(generalCard,
                                            tr("Theme"),
                                            tr("Choose the color scheme for the app."));
            m_themeCombo = new StyledComboBox(row);
            m_themeCombo->addItem(tr("System Settings"), QStringLiteral("system"));
            m_themeCombo->addItem(tr("Dark"),            QStringLiteral("dark"));
            m_themeCombo->addItem(tr("Light"),           QStringLiteral("light"));

            {
                const AppConfig::Theme t = AppConfig::instance().theme();
                const int idx = (t == AppConfig::Theme::Dark)  ? THEME_IDX_DARK  :
                                (t == AppConfig::Theme::Light) ? THEME_IDX_LIGHT : THEME_IDX_SYSTEM;
                m_themeCombo->setCurrentIndex(idx);
            }

            connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this](const int idx)
            {
                const AppConfig::Theme t = (idx == THEME_IDX_DARK)  ? AppConfig::Theme::Dark  :
                                           (idx == THEME_IDX_LIGHT) ? AppConfig::Theme::Light :
                                                                       AppConfig::Theme::System;
                AppConfig::instance().setTheme(t);
                ThemeManager::apply(t);
            });

            rl->addWidget(m_themeCombo);
            generalLayout->addWidget(row);
            addQuickSetting({.id = QuickSettingIds::THEME, .label = tr("Theme"),
                             .section = Section::Appearance, .kind = Kind::Combo, .row = row,
                             .control = m_themeCombo});
        }

        //  Section: Globe
        addSectionHeader(appearanceContent, appearanceContentLayout, tr("Globe"));
        auto [globeCard, globeLayout] = addSectionCard(appearanceContent, appearanceContentLayout);

        //  Globe Animation
        {
            auto [row, rl] = makeSettingRow(globeCard, tr("Animation"), globeDescription());
            QLabel* descLabel = row->findChild<QLabel*>(QStringLiteral("settingsDesc"));
            m_globeCombo = new GlobeAnimationCombo(row);
            connect(m_globeCombo, &GlobeAnimationCombo::changed, this, [this]()
            {
                updateGlobePauseToggle();
                emit globeSettingsChanged();
            });
            // Auto is described only once the desktop's preference turns out
            // to be readable, which is when the combo box starts listing it.
            connect(&MotionPreference::instance(), &MotionPreference::changed, descLabel, [descLabel]()
            {
                descLabel->setText(globeDescription());
            });

            rl->addWidget(m_globeCombo);
            globeLayout->addWidget(row);
            addQuickSetting({.id = QuickSettingIds::GLOBE_ANIMATION, .label = tr("Globe Animation"),
                             .section = Section::Appearance, .kind = Kind::Combo, .row = row,
                             .control = m_globeCombo});
        }

        //  Pause Globe When Unfocused
        {
            addDivider(globeLayout, globeCard);
            auto [row, rl] = makeSettingRow(globeCard,
                                            tr("Pause When Unfocused"),
                                            tr("Stop the globe from spinning while another window has focus, "
                                               "to save CPU."));
            m_globePauseToggle = new ToggleWithStatus(row);
            m_globePauseToggle->setOn(AppConfig::instance().globePauseWhenUnfocused(), false);
            updateGlobePauseToggle();
            connect(m_globePauseToggle, &ToggleWithStatus::toggled, this, [this](const bool on)
            {
                AppConfig::instance().setGlobePauseWhenUnfocused(on);
                emit globeSettingsChanged();
            });
            rl->addWidget(m_globePauseToggle);
            globeLayout->addWidget(row);
            addQuickSetting({.id = QuickSettingIds::GLOBE_PAUSE, .label = tr("Pause Globe When Unfocused"),
                             .section = Section::Appearance, .row = row, .control = m_globePauseToggle});
        }

        //  Section: Main Page
        addSectionHeader(appearanceContent, appearanceContentLayout, tr("Main Page"));
        auto [mainPageCard, mainPageLayout] = addSectionCard(appearanceContent, appearanceContentLayout);

        //  Show Countries
        {
            auto [row, rl] = makeSettingRow(mainPageCard,
                                            tr("Show Countries"),
                                            tr("Show the Countries list beside the main VPN page when the "
                                               "window is wide enough."));
            ToggleWithStatus* toggle = new ToggleWithStatus(row);
            toggle->setOn(AppConfig::instance().showCountriesSidebar(), false);
            connect(toggle, &ToggleWithStatus::toggled, this, [this](const bool on)
            {
                AppConfig::instance().setShowCountriesSidebar(on);
                emit countriesSidebarVisibilityChanged(on);
            });
            rl->addWidget(toggle);
            mainPageLayout->addWidget(row);
            addQuickSetting({.id = QuickSettingIds::SHOW_COUNTRIES, .label = tr("Show Countries"),
                             .section = Section::Appearance, .row = row, .control = toggle});
        }

        //  Show Quick Settings
        {
            addDivider(mainPageLayout, mainPageCard);
            auto [row, rl] = makeSettingRow(mainPageCard,
                                            tr("Show Quick Settings"),
                                            tr("Show quick settings beside the main VPN page when the window "
                                               "is wide enough. Right-click any setting to add it to them or "
                                               "take it out, up to %1.").arg(AppConfig::QUICK_SETTINGS_MAX));
            ToggleWithStatus* toggle = new ToggleWithStatus(row);
            toggle->setOn(AppConfig::instance().showQuickSettings(), false);
            connect(toggle, &ToggleWithStatus::toggled, this, [this](const bool on)
            {
                AppConfig::instance().setShowQuickSettings(on);
                emit quickSettingsChanged();
            });
            rl->addWidget(toggle);
            mainPageLayout->addWidget(row);
            addQuickSetting({.id = QuickSettingIds::SHOW_QUICK_SETTINGS, .label = tr("Show Quick Settings"),
                             .section = Section::Appearance, .row = row, .control = toggle});
        }

        //  Restore Default Quick Settings
        {
            addDivider(mainPageLayout, mainPageCard);
            auto [row, rl] = makeSettingRow(mainPageCard,
                                            tr("Default Quick Settings"),
                                            tr("Put back the quick settings the app starts with."));
            QPushButton* restoreBtn = new QPushButton(tr("Restore"), row);
            restoreBtn->setObjectName(QStringLiteral("secondaryButton"));
            restoreBtn->setCursor(Qt::PointingHandCursor);
            connect(restoreBtn, &QPushButton::clicked, this, [this]()
            {
                AppConfig::instance().setQuickSettings(AppConfig::defaultQuickSettings());
                emit quickSettingsChanged();
                ToastNotification::popup(this, tr("Quick settings restored to their defaults."));
            });
            rl->addWidget(restoreBtn);
            mainPageLayout->addWidget(row);
        }

        //  Section: Main Page Dropdowns
        addSectionHeader(appearanceContent, appearanceContentLayout, tr("Main Page Dropdowns"));
        auto [dropdownCard, dropdownLayout] = addSectionCard(appearanceContent, appearanceContentLayout);

        //  Show Selected Location Picker
        {
            auto [row, rl] = makeSettingRow(dropdownCard,
                                            tr("Show Selected Location"),
                                            tr("Display the Selected Location dropdown on the main VPN page."));
            ToggleWithStatus* toggle = new ToggleWithStatus(row);
            toggle->setOn(AppConfig::instance().showLocationPicker(), false);
            connect(toggle, &ToggleWithStatus::toggled, this, [this](const bool on)
            {
                AppConfig::instance().setShowLocationPicker(on);
                emit locationPickerVisibilityChanged(on);
            });
            rl->addWidget(toggle);
            dropdownLayout->addWidget(row);
            addQuickSetting({.id = QuickSettingIds::SHOW_LOCATION_PICKER, .label = tr("Show Selected Location"),
                             .section = Section::Appearance, .row = row, .control = toggle});
        }

        //  Show Favorites Dropdown
        {
            addDivider(dropdownLayout, dropdownCard);
            auto [row, rl] = makeSettingRow(dropdownCard,
                                            tr("Show Favorites Dropdown"),
                                            tr("Display the Favorites dropdown on the main VPN page."));
            ToggleWithStatus* toggle = new ToggleWithStatus(row);
            toggle->setOn(AppConfig::instance().showFavoritesDropdown(), false);
            connect(toggle, &ToggleWithStatus::toggled, this, [this](const bool on)
            {
                AppConfig::instance().setShowFavoritesDropdown(on);
                emit favoritesDropdownVisibilityChanged(on);
            });
            rl->addWidget(toggle);

            m_showFavoritesDropdownRow    = row;
            m_showFavoritesDropdownToggle = toggle;

            if (AppConfig::instance().favoritesEnabled() == false)
            {
                const QString tip = tr("Enable the Favorites system to use this setting.");
                toggle->setEnabled(false);
                row->setToolTip(tip);
                toggle->setToolTip(tip);
            }

            dropdownLayout->addWidget(row);
            addQuickSetting({.id = QuickSettingIds::SHOW_FAVORITES_DROPDOWN, .label = tr("Show Favorites Dropdown"),
                             .section = Section::Appearance, .row = row, .control = toggle});
        }

        appearanceContentLayout->addStretch();
    }
}

void SettingsPage::updateGlobePauseToggle() const
{
    // Nothing to pause with the globe off.
    m_globePauseToggle->setEnabled(AppConfig::instance().globeAnimation() != AppConfig::GlobeAnimation::Off);
}

// static
QString SettingsPage::globeDescription()
{
    if (MotionPreference::instance().support() == MotionPreference::Support::Available)
    {
        return tr("Show a slowly spinning globe behind the connect button that turns to face the "
                  "country you connect to. Auto follows your desktop's reduce-motion setting. "
                  "Off removes it.");
    }
    return tr("Show a slowly spinning globe behind the connect button that turns to face the "
              "country you connect to. Off removes it.");
}

// VPN tab: everything applied through `protonvpn config set`.
void SettingsPage::buildVpnTab(QTabWidget* tabs)
{
    // ============================================================
    // TAB 3 – VPN
    // ============================================================
    QWidget* vpnTab = new QWidget();
    auto [vpnCard, vpnCardLayout] = makeTabCard(vpnTab);
    m_vpnCard = vpnCard;

    {
        QScrollArea* scroll = vpnTab->findChild<QScrollArea*>();
        scroll->takeWidget();
        QWidget* wrapper = new QWidget();
        QVBoxLayout* wl = new QVBoxLayout(wrapper);
        wl->setContentsMargins(0, WRAPPER_TOP_MARGIN, 0, 0);
        wl->setSpacing(0);
        wl->addWidget(vpnCard, 1);
        scroll->setWidget(wrapper);
        scroll->setWidgetResizable(true);
    }

    {
        QVBoxLayout* vpnPageLayout = qobject_cast<QVBoxLayout*>(vpnTab->layout());
        m_statusLabel = new QLabel(vpnTab);
        m_statusLabel->setAlignment(Qt::AlignCenter);
        m_statusLabel->setObjectName(QStringLiteral("settingsStatusLabel"));
        m_statusLabel->setVisible(false);
        vpnPageLayout->insertWidget(0, m_statusLabel);
    }

    {
        m_refreshBtn = new QPushButton(tr("↻ Refresh"), vpnTab);
        m_refreshBtn->setObjectName(QStringLiteral("refreshOverlayButton"));
        m_refreshBtn->setFixedHeight(REFRESH_BTN_HEIGHT);
        connect(m_refreshBtn, &QPushButton::clicked, this, &SettingsPage::refresh);
        m_refreshBtn->raise();

        m_vpnTabWidget = vpnTab;
        vpnTab->installEventFilter(this);
    }

    tabs->addTab(vpnTab, tr("VPN"));

    m_cliControls = new CliSettingControls(m_manager, this);
    connect(m_cliControls, &CliSettingControls::reconnectStarted, this, [this]()
    {
        m_sequencePending = true;
    });

    using Section = QuickSettingSource::Section;
    using Kind = QuickSettingSource::Kind;
    // Offers a row made by makeToggleRow() or makeComboRow() to the quick
    // settings, and returns it.
    auto offer = [this](QWidget* row, const QString& id, const QString& label, const Kind kind,
                        const bool plusOnly) -> QWidget*
    {
        QWidget* control = kind == Kind::Combo ? static_cast<QWidget*>(row->findChild<QComboBox*>())
                                               : static_cast<QWidget*>(row->findChild<ToggleWithStatus*>());
        addQuickSetting({.id = id, .label = label, .section = Section::Vpn, .kind = kind, .row = row,
                         .control = control, .plusOnly = plusOnly});
        return row;
    };

    bool vpnFirst = true;
    auto addVpn = [&](QWidget* w)
    {
        if (vpnFirst == false)
        {
            addDivider(vpnCardLayout, vpnCard);
        }
        vpnFirst = false;
        vpnCardLayout->addWidget(w);
    };

    //  Anonymous Crash Reports
    addVpn(offer(makeToggleRow(vpnCard,
                               tr("Anonymous Crash Reports"),
                               tr("Send anonymous crash reports to Proton for the VPN CLI tool - not this Qt app."),
                               QStringLiteral("anonymous-crash-reports")),
                 QuickSettingIds::CRASH_REPORTS, tr("Crash Reports"), Kind::Toggle, false));

    //  IPv6
    addVpn(offer(makeToggleRow(vpnCard,
                               tr("IPv6"),
                               tr("Enable IPv6 support over the VPN tunnel."),
                               QStringLiteral("ipv6"),
                               QStringLiteral("on"),
                               /*requiresReconnect=*/true),
                 QuickSettingIds::IPV6, tr("IPv6"), Kind::Toggle, false));

    //  Kill Switch
    {
        QWidget* ksContainer = new QWidget(vpnCard);
        QVBoxLayout* ksVLayout = new QVBoxLayout(ksContainer);
        ksVLayout->setContentsMargins(0, 0, 0, 0);
        ksVLayout->setSpacing(0);

        auto [ksRow, ksRowLayout] = makeSettingRow(ksContainer,
                                                   tr("Kill Switch"),
                                                   tr("Block internet access if the VPN connection drops unexpectedly."));
        m_killSwitchToggle = new ToggleWithStatus(ksRow);
        ksRowLayout->addWidget(m_killSwitchToggle, 0);
        ksVLayout->addWidget(ksRow);

        m_killSwitchSubPanel = new QWidget(ksContainer);
        m_killSwitchSubPanel->setVisible(false);
        QVBoxLayout* spLayout = new QVBoxLayout(m_killSwitchSubPanel);
        spLayout->setContentsMargins(KS_SUBPANEL_H_MARGIN, 0, KS_SUBPANEL_H_MARGIN, KS_SUBPANEL_BOT_MARGIN);
        spLayout->setSpacing(0);

        QFrame* spSep = new QFrame(m_killSwitchSubPanel);
        spSep->setFrameShape(QFrame::HLine);
        spSep->setObjectName(QStringLiteral("divider"));
        spLayout->addWidget(spSep);

        QButtonGroup* radioGroup = new QButtonGroup(m_killSwitchSubPanel);

        auto makeKsOption = [&](const QString& title, const QString& desc,
                                const bool enabled, const bool checked) -> QRadioButton*
        {
            QWidget* optRow = new QWidget(m_killSwitchSubPanel);
            QHBoxLayout* hl = new QHBoxLayout(optRow);
            hl->setContentsMargins(0, KS_OPTION_V_MARGIN, 0, KS_OPTION_V_MARGIN);
            hl->setSpacing(KS_OPTION_H_SPACING);

            QRadioButton* radio = new QRadioButton(optRow);
            radio->setChecked(checked);
            radio->setEnabled(enabled);
            hl->addWidget(radio, 0, Qt::AlignTop);

            QWidget* textCol = new QWidget(optRow);
            QVBoxLayout* vl = new QVBoxLayout(textCol);
            vl->setContentsMargins(0, 0, 0, 0);
            vl->setSpacing(KS_TEXT_COL_SPACING);

            QLabel* titleLbl = new QLabel(title, textCol);
            titleLbl->setObjectName(QStringLiteral("infoKey"));
            titleLbl->setProperty("disabled", enabled == false);
            vl->addWidget(titleLbl);

            QLabel* descLbl = new QLabel(desc, textCol);
            descLbl->setObjectName(QStringLiteral("settingsDesc"));
            descLbl->setWordWrap(true);
            QFont f = descLbl->font();
            f.setPointSize(qMax(f.pointSize() - DESC_FONT_REDUCTION, DESC_FONT_MIN_SIZE));
            descLbl->setFont(f);
            descLbl->setProperty("disabled", enabled == false);
            vl->addWidget(descLbl);

            hl->addWidget(textCol, 1);
            spLayout->addWidget(optRow);
            return radio;
        };

        QRadioButton* standardRadio = makeKsOption(
            tr("Standard"),
            tr("Automatically disconnect from the internet if the VPN connection is lost."),
            true, true);
        radioGroup->addButton(standardRadio);

        QFrame* optSep = new QFrame(m_killSwitchSubPanel);
        optSep->setFrameShape(QFrame::HLine);
        optSep->setObjectName(QStringLiteral("divider"));
        spLayout->addWidget(optSep);

        QRadioButton* advancedRadio = makeKsOption(
            tr("Advanced"),
            tr("Only allow internet access when connected to ProtonVPN. "
               "Advanced kill switch will remain active even when you restart your device."),
            false, false);
        radioGroup->addButton(advancedRadio);
        const QString advTooltip = tr(
            "Temporarily removed from the Proton VPN CLI - not currently available.");
        advancedRadio->setToolTip(advTooltip);
        advancedRadio->parentWidget()->setToolTip(advTooltip);

        ksVLayout->addWidget(m_killSwitchSubPanel);

        connect(m_killSwitchToggle, &ToggleWithStatus::toggled, this, [this](const bool on)
        {
            if (m_manager->currentState() != VpnState::Disconnected)
            {
                m_killSwitchToggle->blockSignals(true);
                m_killSwitchToggle->setOn(on == false, false);
                m_killSwitchToggle->blockSignals(false);
                m_killSwitchSubPanel->setVisible(on == false);

                showReconnectDialog(tr("Kill Switch"), [this, on]()
                {
                    m_killSwitchToggle->blockSignals(true);
                    m_killSwitchToggle->setOn(on, true);
                    m_killSwitchToggle->blockSignals(false);
                    m_killSwitchSubPanel->setVisible(on);
                    m_sequencePending = true;
                    const QString val = on == true ? QStringLiteral("standard")
                                                   : QStringLiteral("off");
                    m_manager->applyConfigValueAndReconnect(
                        QStringLiteral("kill-switch"), val);
                });
                return;
            }

            m_killSwitchSubPanel->setVisible(on);
            m_manager->applyConfigValue(QStringLiteral("kill-switch"),
                                        on == true ? QStringLiteral("standard") : QStringLiteral("off"));
        });

        addVpn(ksContainer);
        addQuickSetting({.id = QuickSettingIds::KILL_SWITCH, .label = tr("Kill Switch"),
                         .section = Section::Vpn, .row = ksRow, .control = m_killSwitchToggle});
    }

    //  Plus Members Only divider
    addDivider(vpnCardLayout, vpnCard);
    m_plusDivider = new PlusDivider(vpnCard);
    vpnCardLayout->addWidget(m_plusDivider);

    m_plusSection = new QWidget(vpnCard);
    QVBoxLayout* plusLayout = new QVBoxLayout(m_plusSection);
    plusLayout->setContentsMargins(0, 0, 0, 0);
    plusLayout->setSpacing(0);
    vpnCardLayout->addWidget(m_plusSection);

    bool plusFirst = true;
    auto addPlus = [&](QWidget* w)
    {
        if (plusFirst == false)
        {
            addDivider(plusLayout, m_plusSection);
        }
        plusFirst = false;
        plusLayout->addWidget(w);
    };

    //  NAT Type
    addPlus(offer(makeComboRow(m_plusSection,
                               tr("NAT Type"),
                               tr("Controls how the VPN server maps your connection. "
                                  "<b>Strict (Type 3)</b> is the default and best for privacy. "
                                  "<b>Moderate (Type 2)</b> improves compatibility for online gaming and WebRTC, "
                                  "at a slight privacy trade-off. "
                                  "<a href='https://protonvpn.com/support/moderate-nat'>Read more</a>"),
                               QStringLiteral("moderate-nat"),
                               {tr("Strict (Type 3)"),
                                tr("Moderate (Type 2)")},
                               {QStringLiteral("off"), QStringLiteral("on")},
                               /*requiresReconnect=*/true),
                  QuickSettingIds::NAT_TYPE, tr("NAT Type"), Kind::Combo, true));

    //  VPN Accelerator
    addPlus(offer(makeToggleRow(m_plusSection,
                                tr("VPN Accelerator"),
                                tr("Boost connection speeds using advanced protocol techniques."),
                                QStringLiteral("vpn-accelerator")),
                  QuickSettingIds::VPN_ACCELERATOR, tr("VPN Accelerator"), Kind::Toggle, true));

    //  NetShield
    addPlus(offer(makeComboRow(m_plusSection,
                               tr("NetShield Ad-blocker"),
                               tr("Block malware, ads, and trackers at the DNS level."),
                               QStringLiteral("netshield"),
                               {
                                   tr("Off"),
                                   tr("Malware only"),
                                   tr("Malware, ads & trackers")
                               },
                               {
                                   QStringLiteral("off"),
                                   QStringLiteral("malware-only"),
                                   QStringLiteral("malware-ads-trackers")
                               }),
                  QuickSettingIds::NETSHIELD, tr("NetShield"), Kind::Combo, true));

    //  Port Forwarding
    {
        QWidget* pfRow = makeToggleRow(m_plusSection,
                                       tr("Port Forwarding"),
                                       tr("Bypass firewalls to connect to P2P servers and devices in your local network. "
                                          "<a href='https://protonvpn.com/support/port-forwarding'>Learn more</a>"),
                                       QStringLiteral("port-forwarding"));
        m_portForwardingToggle = pfRow->findChild<ToggleWithStatus*>();

        connect(m_portForwardingToggle, &ToggleWithStatus::toggled, this, [this](const bool on)
        {
            if (on == true && NatPmpManager::isInstalled() == false)
            {
                NatpmpcDialog dlg(this);
                dlg.exec();
            }
        });

        connect(m_portForwardingToggle, &ToggleWithStatus::toggled, this, [this](const bool on)
        {
            if (on == false && m_settingsPortRow != nullptr)
            {
                m_settingsPortRow->setVisible(false);
            }
        });

        addPlus(pfRow);
        addQuickSetting({.id = QuickSettingIds::PORT_FORWARDING, .label = tr("Port Forwarding"),
                         .section = Section::Vpn, .row = pfRow, .control = m_portForwardingToggle,
                         .plusOnly = true});

        m_settingsPortRow = new QWidget(m_plusSection);
        m_settingsPortRow->setObjectName(QStringLiteral("settingsPortRow"));
        QHBoxLayout* portRowLayout = new QHBoxLayout(m_settingsPortRow);
        portRowLayout->setContentsMargins(PORT_ROW_H_MARGIN, 0, PORT_ROW_H_MARGIN, PORT_ROW_BOT_MARGIN);
        portRowLayout->setSpacing(PORT_ROW_SPACING);

        QLabel* portTitleLabel = new QLabel(tr("Forwarded Port:"), m_settingsPortRow);
        portTitleLabel->setObjectName(QStringLiteral("infoLabel"));
        portRowLayout->addWidget(portTitleLabel, 0, Qt::AlignVCenter);

        QWidget* btnGroup = new QWidget(m_settingsPortRow);
        QHBoxLayout* btnGroupLayout = new QHBoxLayout(btnGroup);
        btnGroupLayout->setContentsMargins(0, 0, 0, 0);
        btnGroupLayout->setSpacing(0);

        m_settingsPortLabel = new QLabel(QStringLiteral("-"), btnGroup);
        m_settingsPortLabel->setObjectName(QStringLiteral("portValueLabel"));
        m_settingsPortLabel->setAlignment(Qt::AlignCenter);
        {
            QFont f = m_settingsPortLabel->font();
            f.setBold(true);
            f.setPointSize(f.pointSize() + PORT_FONT_SIZE_BOOST);
            m_settingsPortLabel->setFont(f);
        }
        btnGroupLayout->addWidget(m_settingsPortLabel);

        QPixmap clipPix(PORT_COPY_ICON_SIZE, PORT_COPY_ICON_SIZE);
        clipPix.fill(Qt::transparent);
        {
            QPainter clipPainter(&clipPix);
            QSvgRenderer clipRenderer(QStringLiteral(":/assets/clipboard2-plus.svg"));
            clipRenderer.render(&clipPainter);
            clipPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            clipPainter.fillRect(clipPix.rect(), Qt::white);
        }
        QPushButton* portCopyBtn = new QPushButton(btnGroup);
        portCopyBtn->setObjectName(QStringLiteral("portCopyBtn"));
        portCopyBtn->setIcon(QIcon(clipPix));
        portCopyBtn->setIconSize({PORT_COPY_ICON_SIZE, PORT_COPY_ICON_SIZE});
        portCopyBtn->setFixedSize(PORT_COPY_BTN_W, PORT_COPY_BTN_H);
        portCopyBtn->setCursor(Qt::PointingHandCursor);
        portCopyBtn->setToolTip(tr("Copy to Clipboard"));
        connect(portCopyBtn, &QPushButton::clicked, this, [this]()
        {
            if (m_natPmpManager != nullptr && m_natPmpManager->forwardedPort() > 0)
            {
                QApplication::clipboard()->setText(QString::number(m_natPmpManager->forwardedPort()));
            }
        });
        btnGroupLayout->addWidget(portCopyBtn);

        portRowLayout->addWidget(btnGroup, 0, Qt::AlignVCenter);
        portRowLayout->addStretch();
        plusLayout->addWidget(m_settingsPortRow);
        m_settingsPortRow->setVisible(false);

        if (m_natPmpManager != nullptr)
        {
            connect(m_natPmpManager, &NatPmpManager::portAcquired, this, [this](const int port)
            {
                if (m_settingsPortLabel != nullptr)
                {
                    m_settingsPortLabel->setText(QString::number(port));
                }
                if (m_settingsPortRow != nullptr)
                {
                    m_settingsPortRow->setVisible(true);
                }
            });
            connect(m_natPmpManager, &NatPmpManager::portLost, this, [this]()
            {
                if (m_settingsPortRow != nullptr)
                {
                    m_settingsPortRow->setVisible(false);
                }
            });
            connect(m_natPmpManager, &NatPmpManager::natpmpcMissing, this, [this]()
            {
                if (m_settingsPortRow != nullptr)
                {
                    m_settingsPortRow->setVisible(false);
                }
            });
            if (m_natPmpManager->forwardedPort() > 0)
            {
                m_settingsPortLabel->setText(QString::number(m_natPmpManager->forwardedPort()));
                m_settingsPortRow->setVisible(true);
            }
        }
    }

    //  Custom DNS
    {
        addDivider(plusLayout, m_plusSection);

        auto [dnsRow, dnsRl] = makeSettingRow(m_plusSection,
                                              tr("Custom DNS"),
                                              tr("Override the VPN DNS with your own resolver(s). "
                                                 "Separate multiple addresses with a comma."));
        // Tighter at the bottom: the server field sits right below.
        dnsRl->setContentsMargins(SETTING_ROW_H_MARGIN, SETTING_ROW_V_MARGIN,
                                  SETTING_ROW_H_MARGIN, DNS_ADDR_BOT_MARGIN / 3);
        m_dnsToggle = new ToggleWithStatus(dnsRow);
        dnsRl->addWidget(m_dnsToggle);
        plusLayout->addWidget(dnsRow);

        QWidget* dnsAddrRow = new QWidget(m_plusSection);
        m_dnsAddrRow = dnsAddrRow;
        dnsAddrRow->setVisible(false);
        QHBoxLayout* dnsAddrRl = new QHBoxLayout(dnsAddrRow);
        dnsAddrRl->setContentsMargins(SETTING_ROW_H_MARGIN, 0, SETTING_ROW_H_MARGIN, DNS_ADDR_BOT_MARGIN);
        m_dnsEdit = new QLineEdit(dnsAddrRow);
        m_dnsEdit->setPlaceholderText(QStringLiteral("e.g. 1.1.1.1,8.8.8.8"));
        m_dnsEdit->setObjectName(QStringLiteral("settingsDnsEdit"));
        dnsAddrRl->addWidget(m_dnsEdit);
        m_dnsApplyBtn = new QPushButton(tr("Apply"), dnsAddrRow);
        m_dnsApplyBtn->setObjectName(QStringLiteral("secondaryButton"));
        m_dnsApplyBtn->setFixedHeight(DNS_APPLY_BTN_HEIGHT);
        dnsAddrRl->addWidget(m_dnsApplyBtn);
        plusLayout->addWidget(dnsAddrRow);
        addQuickSetting({.id = QuickSettingIds::CUSTOM_DNS, .label = tr("Custom DNS"),
                         .section = Section::Vpn, .kind = Kind::CustomDns, .row = dnsRow,
                         .control = m_dnsToggle, .dnsEdit = m_dnsEdit, .dnsApply = m_dnsApplyBtn,
                         .plusOnly = true});

        connect(m_dnsToggle, &ToggleWithStatus::toggled, this, [this, dnsAddrRow](const bool on)
        {
            if (on == false && m_manager->currentState() != VpnState::Disconnected)
            {
                m_dnsToggle->blockSignals(true);
                m_dnsToggle->setOn(true, false);
                m_dnsToggle->blockSignals(false);

                showReconnectDialog(tr("Custom DNS"), [this, dnsAddrRow]()
                {
                    dnsAddrRow->setVisible(false);
                    m_dnsToggle->blockSignals(true);
                    m_dnsToggle->setOn(false, true);
                    m_dnsToggle->blockSignals(false);
                    m_sequencePending = true;
                    m_manager->applyConfigValueAndReconnect(QStringLiteral("custom-dns"), QStringLiteral("off"));
                });
                return;
            }

            dnsAddrRow->setVisible(on);
            if (on == false)
            {
                m_manager->applyConfigValue(QStringLiteral("custom-dns"), QStringLiteral("off"));
            }
        });

        connect(m_dnsApplyBtn, &QPushButton::clicked, this, [this]()
        {
            const QString dns = CliSettings::normalizeDnsList(m_dnsEdit->text());
            if (dns.isEmpty()) return;

            const QString cliValue = QStringLiteral("--dns %1 on").arg(dns);

            if (m_manager->currentState() != VpnState::Disconnected)
            {
                showReconnectDialog(tr("Custom DNS"), [this, cliValue]()
                {
                    m_sequencePending = true;
                    m_manager->applyConfigValueAndReconnect(
                        QStringLiteral("custom-dns"), cliValue);
                });
                return;
            }

            m_manager->applyConfigValue(QStringLiteral("custom-dns"), cliValue);
        });
    }

    vpnCardLayout->addStretch();
}


void SettingsPage::updateAutoConnectRowVisibility() const
{
    if (m_autoConnectRow == nullptr) return;
    const bool show = m_autoStartToggle != nullptr && m_autoStartToggle->isOn();
    m_autoConnectRow->setVisible(show);
    // If auto-start is turned off, also disable auto-connect and persist that.
    if (show == false && m_autoConnectToggle != nullptr && m_autoConnectToggle->isOn())
    {
        m_autoConnectToggle->setOn(false, false);
        AppConfig::instance().setAutoConnect(false);
    }
    updateAutoConnectServerRow();
}

void SettingsPage::populateAutoConnectServerCombo() const
{
    if (m_autoConnectServerCombo == nullptr) return;

    const QString saved = AppConfig::instance().autoConnectServer();

    const QSignalBlocker blocker(m_autoConnectServerCombo);
    m_autoConnectServerCombo->clear();
    m_autoConnectServerCombo->addItem(tr("Fastest Server"), QString());

    const QList<FavoriteEntry> favs = FavoritesManager::instance().entries();
    for (const FavoriteEntry& e : favs)
    {
        const QString key = e.city.isEmpty()
            ? e.countryCode
            : e.countryCode + QStringLiteral("|") + e.city;
        const QString display = e.city.isEmpty()
            ? tr("%1 - Fastest").arg(e.countryName)
            : tr("%1 - %2").arg(e.countryName, e.city);
        m_autoConnectServerCombo->addItem(display, key);
    }

    // Restore saved selection (or stay on index 0 if not found).
    int idx = 0;
    for (int i = 1; i < m_autoConnectServerCombo->count(); ++i)
    {
        if (m_autoConnectServerCombo->itemData(i).toString() == saved)
        {
            idx = i;
            break;
        }
    }
    m_autoConnectServerCombo->setCurrentIndex(idx);
}

void SettingsPage::updateAutoConnectServerRow() const
{
    if (m_autoConnectServerRow == nullptr) return;

    const bool autoConnectOn = m_autoConnectToggle != nullptr && m_autoConnectToggle->isOn();
    const bool autoStartOn   = m_autoStartToggle   != nullptr && m_autoStartToggle->isOn();
    const bool show          = autoStartOn && autoConnectOn;
    m_autoConnectServerRow->setVisible(show);

    if (m_autoConnectServerCombo == nullptr) return;

    const bool hasFavorites = FavoritesManager::instance().hasAnyEntries();
    m_autoConnectServerRow->setEnabled(hasFavorites);
    if (hasFavorites == false)
    {
        const QString tip = tr("Add favorite servers to choose a specific server for auto-connect.");
        m_autoConnectServerRow->setToolTip(tip);
        m_autoConnectServerCombo->setToolTip(tip);
    }
    else
    {
        m_autoConnectServerRow->setToolTip(QString());
        m_autoConnectServerCombo->setToolTip(QString());
    }
}

SettingsPage::SettingsPage(VpnManager* manager, NatPmpManager* natPmpManager, QWidget* parent)
    : QWidget(parent), m_manager(manager), m_natPmpManager(natPmpManager)
{
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(OUTER_LAYOUT_MARGIN, OUTER_LAYOUT_MARGIN,
                                    OUTER_LAYOUT_MARGIN, OUTER_LAYOUT_MARGIN);
    outerLayout->setSpacing(OUTER_LAYOUT_SPACING);

    QLabel* titleLabel = new QLabel(tr("Settings"), this);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    outerLayout->addWidget(titleLabel);

    QTabWidget* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("settingsTabs"));
    outerLayout->addWidget(tabs, 1);

    buildAppTab(tabs);
    buildAppearanceTab(tabs);
    buildVpnTab(tabs);

    updatePlusSectionState();

    connect(m_manager, &VpnManager::settingsReady,    this, &SettingsPage::onSettingsReady);
    connect(m_manager, &VpnManager::accountTypeReady, this, [this](AccountType)
    {
        updatePlusSectionState();
    });
    connect(m_manager, &VpnManager::cliVersionReady,  this, [this](const QString& v)
    {
        m_installedCliVersion = v;
    });
    connect(m_manager, &VpnManager::configApplied, this, &SettingsPage::maybeWarnReconnect);

    connect(m_manager, &VpnManager::connectionStateChanged, this,
            [this](const VpnState state, const QString&)
    {
        const bool transitioning = state == VpnState::Connecting
                                || state == VpnState::Disconnecting;

        if (m_sequencePending == true && transitioning == false)
        {
            if (state == VpnState::Connected || state == VpnState::Error)
            {
                m_sequencePending = false;
                refresh();
            }
            else
            {
                if (m_vpnCard != nullptr)    { m_vpnCard->setEnabled(false); }
                if (m_refreshBtn != nullptr) { m_refreshBtn->setEnabled(false); }
            }
            return;
        }

        if (m_vpnCard != nullptr)    { m_vpnCard->setEnabled(transitioning == false); }
        if (m_refreshBtn != nullptr) { m_refreshBtn->setEnabled(transitioning == false); }
        if (transitioning == false)
        {
            updatePlusSectionState();
        }
    });

    {
        const VpnState s = m_manager->currentState();
        const bool transitioning = s == VpnState::Connecting
                                || s == VpnState::Disconnecting;
        if (transitioning == true)
        {
            if (m_vpnCard != nullptr)    { m_vpnCard->setEnabled(false); }
            if (m_refreshBtn != nullptr) { m_refreshBtn->setEnabled(false); }
        }
    }

    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(SPINNER_INTERVAL_MS);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this]()
    {
        m_spinnerFrame = (m_spinnerFrame + 1) % kSpinnerFrameCount;
        m_statusLabel->setText(
            tr("%1 Loading settings\u2026").arg(QString::fromUtf8(kSpinnerFrames[m_spinnerFrame])));
    });
}

// ============================================================
// SettingsPage slots
// ============================================================

void SettingsPage::refresh()
{
    setLoading(true);
    m_manager->fetchSettings();
}

bool SettingsPage::eventFilter(QObject* obj, QEvent* event)
{
    // Keep the floating Refresh button anchored to the top-right of the VPN tab.
    if (obj == m_vpnTabWidget && m_refreshBtn != nullptr
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
    {
        const int btnW = m_refreshBtn->sizeHint().width();
        m_refreshBtn->setGeometry(m_vpnTabWidget->width() - btnW - REFRESH_OVERLAY_MARGIN,
                                  REFRESH_OVERLAY_MARGIN, btnW, REFRESH_OVERLAY_H);
        m_refreshBtn->raise();
    }

    // Fire the About dialog when the user clicks the About nav row.
    if (obj == m_aboutRow && event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton
            && m_aboutRow->rect().contains(me->position().toPoint()))
        {
            AboutDialog dlg(m_installedCliVersion, this);
            dlg.exec();
        }
    }

    return QWidget::eventFilter(obj, event);
}

void SettingsPage::updatePlusSectionState() const
{
    const bool isFree = m_manager->accountType() == AccountType::Free;

    auto applyToSection = [&](QWidget* section, QWidget* divider)
    {
        if (section == nullptr || divider == nullptr) return;
        divider->setVisible(isFree);
        section->setEnabled(isFree == false);
        if (isFree == true)
        {
            QGraphicsOpacityEffect* effect = qobject_cast<QGraphicsOpacityEffect*>(section->graphicsEffect());
            if (effect == nullptr)
            {
                effect = new QGraphicsOpacityEffect(section);
                section->setGraphicsEffect(effect);
            }
            effect->setOpacity(PLUS_SECTION_DISABLED_OPACITY);
        }
        else
        {
            section->setGraphicsEffect(nullptr);
        }
    };

    applyToSection(m_plusSection,    m_plusDivider);
    applyToSection(m_appPlusSection, m_appPlusDivider);
}

void SettingsPage::setLoading(const bool loading)
{
    m_loading = loading;
    m_refreshBtn->setEnabled(loading == false);
    m_refreshBtn->setText(loading == true ? tr("Loading\u2026") : tr("↻ Refresh"));
    m_statusLabel->setVisible(loading);
    if (loading == true)
    {
        m_spinnerFrame = 0;
        m_statusLabel->setText(tr("\u28cb Loading settings\u2026"));
        m_spinnerTimer->start();
    }
    else
    {
        m_spinnerTimer->stop();

        if (m_vpnCard != nullptr)
        {
            const VpnState s = m_manager->currentState();
            const bool transitioning = s == VpnState::Connecting
                                    || s == VpnState::Disconnecting;
            if (transitioning == false)
            {
                m_vpnCard->setEnabled(true);
            }
        }
    }
    const bool controlsEnabled = loading == false;
    m_cliControls->setEnabled(controlsEnabled);
    if (m_killSwitchToggle != nullptr)
    {
        m_killSwitchToggle->setEnabled(controlsEnabled);
    }
    if (m_autoStartToggle != nullptr)
    {
        m_autoStartToggle->setEnabled(controlsEnabled);
    }
    if (m_notificationsToggle != nullptr)
    {
        m_notificationsToggle->setEnabled(controlsEnabled);
    }
    if (m_recentConnectionsSpinBox != nullptr)
    {
        m_recentConnectionsSpinBox->setEnabled(controlsEnabled);
    }
    if (m_dnsToggle != nullptr)
    {
        m_dnsToggle->setEnabled(controlsEnabled);
    }
    if (m_dnsApplyBtn != nullptr)
    {
        m_dnsApplyBtn->setEnabled(controlsEnabled);
    }
}

void SettingsPage::onSettingsReady(const QMap<QString, QString>& info)
{
    setLoading(false);

    auto val = [&](const QString& key)
    {
        return info.value(key).toLower().trimmed();
    };

    if (m_killSwitchToggle != nullptr && m_killSwitchSubPanel != nullptr)
    {
        const bool ksOn = val(QStringLiteral("kill-switch")) == QLatin1String("standard");
        m_killSwitchToggle->blockSignals(true);
        m_killSwitchToggle->setOn(ksOn, false);
        m_killSwitchToggle->blockSignals(false);
        m_killSwitchSubPanel->setVisible(ksOn);
    }

    // Custom DNS
    const QString dns = info.value(QStringLiteral("custom-dns")).trimmed();
    const bool dnsOn = dns.isEmpty() == false
        && dns.toLower() != QLatin1String("disabled")
        && dns.toLower() != QLatin1String("off")
        && dns.toLower() != QLatin1String("none");
    m_dnsToggle->setOn(dnsOn, false);
    // setOn() does not emit toggled(), so the server field has to follow by
    // hand; otherwise a saved custom DNS shows as on with no way to see or
    // edit its servers.
    m_dnsAddrRow->setVisible(dnsOn);
    // "on" alone means enabled with no servers to show.
    if (dnsOn == true && dns.toLower() != QLatin1String("on"))
    {
        m_dnsEdit->setText(dns);
    }
    else
    {
        m_dnsEdit->clear();
    }

    updatePlusSectionState();
}

// ---------------------------------------------------------------------------
// Auto-start helpers (XDG autostart .desktop file)
// ---------------------------------------------------------------------------

QString SettingsPage::autoStartFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/autostart/proton-vpn-qt.desktop");
}

bool SettingsPage::autoStartEnabled()
{
    return QFileInfo::exists(autoStartFilePath());
}

bool SettingsPage::setAutoStart(const bool enable, QString& errorOut)
{
    const QString filePath = autoStartFilePath();

    if (enable == true)
    {
        // Load the bundled .desktop template.
        QFile templateFile(QStringLiteral(":/autostart/proton-vpn-qt.desktop"));
        if (templateFile.open(QIODevice::ReadOnly) == false)
        {
            errorOut = tr("Could not read the autostart template resource.");
            return false;
        }
        QString content = QString::fromUtf8(templateFile.readAll());
        templateFile.close();

        // Substitute the executable path placeholder.
        content.replace(QStringLiteral("@EXEC@"), PlatformUtils::autostartExecCommand());

        if (DRY_RUN_MODE == true)
        {
            DBG_SETTINGS(QStringLiteral("[DRY RUN] Would write autostart file: ") + filePath);
            return true;
        }

        // Ensure ~/.config/autostart/ exists.
        QDir targetDir = QFileInfo(filePath).dir();
        if (targetDir.mkpath(targetDir.absolutePath()) == false)
        {
            errorOut = tr("Could not create the autostart directory.");
            return false;
        }

        QFile outFile(filePath);
        if (outFile.open(QIODevice::WriteOnly | QIODevice::Truncate) == false)
        {
            errorOut = tr("Could not write the autostart file: %1").arg(outFile.errorString());
            return false;
        }
        outFile.write(content.toUtf8());
        outFile.close();
    }
    else
    {
        if (DRY_RUN_MODE == true)
        {
            DBG_SETTINGS(QStringLiteral("[DRY RUN] Would remove autostart file: ") + filePath);
            return true;
        }

        if (QFileInfo::exists(filePath) == true && QFile::remove(filePath) == false)
        {
            errorOut = tr("Could not remove the autostart file: %1").arg(filePath);
            return false;
        }
    }

    return true;
}

