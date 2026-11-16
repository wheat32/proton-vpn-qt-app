#pragma once

#include <optional>
#include <tuple>
#include <QElapsedTimer>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QPropertyAnimation>
#include <QScrollArea>
#include "../vpnManager.h"
#include "../cli/natpmpManager.h"
#include "../widgets/pickerBase.h"
#include "../widgets/pickerDrawer.h"
#include "../widgets/infoBanner.h"
#include "../widgets/pixmapCache.h"
#include "../widgets/svgBanner.h"
#include "../widgets/globeWidget.h"
#include "../widgets/hintBubble.h"
#include <QGridLayout>
#include "../widgets/quickSettingsPanel.h"

// ---------------------------------------------------------------------------
// PowerButton
// ---------------------------------------------------------------------------
class PowerButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal spinAngle READ spinAngle WRITE setSpinAngle)

public:
    explicit PowerButton(QWidget* parent = nullptr);
    enum class RingState { Unknown, Connected, Disconnected, Spinning };
    void setState(RingState s);
    // Resizes the button to `scale` times its design size; the ring, glow and
    // icon all scale with it.
    void setScale(qreal scale);
    [[nodiscard]] qreal spinAngle() const { return m_spinAngle; }
    void setSpinAngle(qreal a) { m_spinAngle = a; update(); }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void focusInEvent(QFocusEvent*) override;
    void focusOutEvent(QFocusEvent*) override;

private:
    RingState m_state = RingState::Unknown;
    qreal m_scale = 1.0;
    // The power icon by size, screen ratio and theme: the background globe
    // repaints the button on every frame.
    PixmapCache<std::tuple<int, qreal, bool>> m_iconCache;
    qreal m_spinAngle = 0.0;
    bool m_hovered = false;
    bool m_pressed = false;
    QPropertyAnimation* m_anim = nullptr;
    void startSpin() const;
    void stopSpin();
};

// ---------------------------------------------------------------------------
// LocationPicker – custom styled dropdown
// ---------------------------------------------------------------------------
class LocationPicker : public PickerBase
{
    Q_OBJECT
public:
    explicit LocationPicker(const QString& countryCode, const QString& countryName, QWidget* parent = nullptr);

    // Fills the list. A city the user had selected stays selected when it is
    // still in `cities`; otherwise the selection goes back to the fastest
    // server.
    void populate(const QList<QPair<QString, QString>>& cities);
    void setLoading(bool loading);
    void setSelectedCity(const QString& city);
    // setSelectedCity() plus the selectionChanged() notification. Use this
    // instead of emitting the signal from outside the class.
    void selectCity(const QString& city);
    // Tries to select city in the populated list.
    // Returns true if found; false if not found (falls back to "Active connection").
    bool trySelectCity(const QString& city);
    void setUnknownConnection(bool unknown);
    // Disables interaction for free-plan users (no popup, forbidden cursor, tooltip).
    void setFreeMode(bool free);

    [[nodiscard]] QString selectedCity() const { return m_selectedCity; }

signals:
    void selectionChanged(const QString& city);
    void changeCountryRequested();

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void onRowClicked(QListWidgetItem* item) override;
    [[nodiscard]] RowLocation rowLocation(const QListWidgetItem* item) const override;

private:
    void updateHeader() const;
    // The row holding `city` (the fastest server's for an empty one), or
    // NO_ROW when the list has none.
    [[nodiscard]] int rowForCity(const QString& city) const;

    static constexpr int NO_ROW = -1;

    QString m_countryCode;
    QString m_countryName;
    QString m_selectedCity;
    bool    m_unknownConnection = false;
    bool    m_freeMode = false;

    QLabel*  m_flagLabel;
    QTimer*  m_loadingTimer = nullptr;
    int      m_loadingFrame = 0;
};

// ---------------------------------------------------------------------------
// SavedConnectionPicker – what the recent and favorites pickers share: rows
// that each stand for a saved connection (a country, with a city or with none
// for its fastest server), and what happens when one is chosen.
// ---------------------------------------------------------------------------
class SavedConnectionPicker : public PickerBase
{
    Q_OBJECT
public:
    explicit SavedConnectionPicker(QWidget* parent = nullptr);

signals:
    void connectionSelected(const QString& countryCode, const QString& city);

protected:
    // Appends the row for one saved connection. An empty `dateText` leaves
    // the date column out.
    void addConnectionRow(const QString& countryCode, const QString& countryName, const QString& city,
                          const QString& dateText, bool withStar);

    bool eventFilter(QObject* obj, QEvent* ev) override;
    void onRowClicked(QListWidgetItem* item) override;
    [[nodiscard]] RowLocation rowLocation(const QListWidgetItem* item) const override;
};

// ---------------------------------------------------------------------------
// RecentPicker – shows recent connections as a dropdown
// ---------------------------------------------------------------------------
class RecentPicker : public SavedConnectionPicker
{
    Q_OBJECT
public:
    explicit RecentPicker(QWidget* parent = nullptr);

    // Reload from ConnectionHistory and rebuild popup list.
    void refresh();

protected:
    void changeEvent(QEvent* event) override;

private:
    // Draws the header's clock in the theme's icon color.
    void updateClockIcon();

    QLabel* m_clockIcon = nullptr;
};

// ---------------------------------------------------------------------------
// FavoritesPicker – shows favorited connections as a dropdown
// ---------------------------------------------------------------------------
class FavoritesPicker : public SavedConnectionPicker
{
    Q_OBJECT
public:
    explicit FavoritesPicker(QWidget* parent = nullptr);

    // Reload from FavoritesManager and rebuild popup list.
    void refresh();
};


// ---------------------------------------------------------------------------
// VpnPage
// ---------------------------------------------------------------------------
class VpnPage : public QWidget
{
    Q_OBJECT

public:
    // Beside the Countries list (MainWindow's split view) the quick settings
    // stay down to a main column this narrow; see setSplitView().
    static constexpr int SPLIT_MIN_MAIN_WIDTH = 400;
    // The narrowest the page gets in the split view, so the quick settings
    // always fit there.
    static constexpr int SPLIT_MIN_WIDTH = SPLIT_MIN_MAIN_WIDTH + QuickSettingsPanel::PANEL_WIDTH;

    explicit VpnPage(VpnManager* manager, QWidget* parent = nullptr);

    void onStateChanged(VpnState state, const QString& info);
    // Applies the globe settings: shows or removes it per Globe Animation and
    // the desktop's reduce-motion preference, and sets Pause When Unfocused.
    void applyGlobeSettings();
    // Points out the power button with a tip, unless one was dismissed before
    // (see AppConfig::powerButtonHintShown()).
    void showPowerButtonHint();
    // A connection to `city` of `countryCode` (the country's fastest server
    // for an empty city) was asked for outside this page, from the Countries
    // list. The Selected Location dropdown follows it only when that is the
    // local country: it lists no other country's cities.
    void notifyExternalConnect(const QString& countryCode, const QString& city);
    void refreshRecentPicker() const;
    // Adds a dismissable warning to the warnings area, which counts it and
    // clears it with the rest. Returns the banner, which deletes itself once
    // dismissed.
    InfoBanner* addWarning(const QString& htmlMessage);
    void refreshFavoritesPicker() const;
    // Called when VpnManager has parsed a city from `protonvpn status`.
    void onStatusCityKnown(const QString& city);
    // Shows or hides the "Selected Location" picker.
    void setLocationPickerVisible(bool visible);
    // Shows or hides the favorites picker (respects hasFavorites + setting).
    void setFavoritesDropdownVisible(bool visible);
    // Called when the favorites-enabled setting changes: favorites off hides
    // their dropdown too.
    void setFavoritesEnabled(bool enabled);
    // The settings the quick settings panel can show: the Settings page's.
    void setQuickSettingSources(const QList<QuickSettingSource>& sources);
    // Shows the quick settings now chosen, or hides them all as asked.
    void updateQuickSettings();
    // Set while the page shares the window with the Countries list. The
    // window is wide then, so the quick settings keep showing beside a narrow
    // main column rather than giving their width back to wide mode.
    void setSplitView(bool split);
    // Returns true while the natpmpc keep-alive loop is running (port forwarding active).
    bool isPortForwardingActive() const { return m_natPmpManager != nullptr && m_natPmpManager->isRunning(); }
    NatPmpManager* natPmpManager() const { return m_natPmpManager; }

signals:
    void connectRequested(const QString& country, const QString& city);
    void disconnectRequested();
    void signOutRequested();
    void changeCountryRequested();

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    void onCitiesReady(const QString& countryCode, const QList<QPair<QString, QString>>& cities);
    void onCliVersionReady(const QString& version);

private:
    VpnManager* m_manager;
    QString m_localCountryCode;
    bool m_isFreeUser = false;

    PowerButton*    m_powerBtn;
    // The first-launch tip on the power button while it shows.
    QPointer<HintBubble> m_powerButtonHint;
    QLabel*         m_statusLabel;
    QLabel*         m_infoLabel;
    QLabel*         m_signOutHintLabel;
    QPushButton*    m_errorDetailsBtn;
    QLabel*         m_timerLabel;
    LocationPicker* m_locationPicker;
    RecentPicker*     m_recentPicker     = nullptr;
    FavoritesPicker*  m_favoritesPicker  = nullptr;
    PickerDrawer*     m_drawer           = nullptr;
    QFrame*           m_drawerNotch      = nullptr;
    QLabel*           m_drawerNotchIcon  = nullptr;
    // Everything but the quick settings panel: the logo, the content and the
    // dropdowns. Its width is mainWidth().
    QWidget*          m_mainColumn       = nullptr;
    QVBoxLayout*      m_outerLayout      = nullptr;
    // The chosen quick settings at the right of the page, shown when the page
    // is wide enough for them (see updateLayoutForSize()).
    QuickSettingsPanel* m_quickSettings  = nullptr;

    // Wide-mode layout widgets
    QWidget*          m_logoRow              = nullptr;
    QHBoxLayout*      m_logoRowLayout        = nullptr;
    SvgBanner*        m_logo                 = nullptr;
    QVBoxLayout*      m_topLayout            = nullptr;
    QWidget*          m_topContentWidget     = nullptr;
    QScrollArea*      m_scrollArea           = nullptr;
    // Narrow-mode scroll offset wrapper - carries the collapsed drawer's width
    // as a left margin so only the scroll area is pushed right (logo/power
    // remain full-width centered).
    QWidget*          m_scrollOffsetWidget   = nullptr;
    QVBoxLayout*      m_scrollOffsetLayout   = nullptr;
    QWidget*          m_narrowContent        = nullptr;
    QVBoxLayout*      m_narrowContentLayout  = nullptr;
    QWidget*          m_wideContent          = nullptr;
    QWidget*          m_pickerSidebar        = nullptr;
    QVBoxLayout*      m_pickerSidebarLayout  = nullptr;
    QWidget*          m_rightContent         = nullptr;
    QVBoxLayout*      m_rightContentLayout   = nullptr;
    // Wide mode places the sidebar and the content in a grid, in one of two
    // arrangements (see updateArrangement()).
    QGridLayout*      m_wideGrid             = nullptr;
    bool              m_wideCentered         = false;
    bool              m_arrangementPending   = false;
    bool              m_wideMode             = false;
    bool              m_splitView            = false; // see setSplitView()

    // The main column's width from which the dropdowns sit expanded beside the
    // content, and the width of the sidebar they sit in.
    static constexpr int WIDE_THRESHOLD     = 700;
    static constexpr int WIDE_SIDEBAR_WIDTH = 300;
    bool            m_showFavoritesDropdown = true; // cached from AppConfig
    bool            m_favoritesEnabled      = true; // cached from AppConfig
    // Container that holds the warning banners added with addWarning() (the
    // pre-release one and whatever MainWindow adds).
    // Moved between the scroll layout and the picker sidebar when switching modes.
    QWidget*      m_vpnBannerArea        = nullptr;
    QWidget*      m_vpnBannerContent     = nullptr;  // direct child in narrow, scroll child in wide
    QVBoxLayout*  m_vpnBannerLayout      = nullptr;
    QScrollArea*  m_vpnBannerScroll      = nullptr;  // created lazily on first wide-mode entry
    QLabel*       m_warningsHeaderLabel  = nullptr;
    QPushButton*  m_clearAllBannersBtn   = nullptr;

    InfoBanner*     m_versionBanner = nullptr;
    InfoBanner*     m_prereleaseBanner = nullptr;
    // Every banner in the warnings area, the pre-release one included.
    QList<InfoBanner*> m_warningBanners;
    QTimer*         m_elapsedTimer;
    QTimer*         m_checkingSpinnerTimer;
    // Monotonic reference for the connection duration display; the label is
    // recomputed from this on every tick rather than incrementing a counter.
    QElapsedTimer   m_connectedSince;
    int   m_checkingSpinnerFrame = 0;
    QString m_rawError;

    // Port forwarding (NatPmpManager keep-alive loop)
    QWidget*        m_portRow       = nullptr;
    QLabel*         m_portLabel     = nullptr;
    NatPmpManager*  m_natPmpManager = nullptr;
    InfoBanner*     m_natpmpcBanner = nullptr;
    // The port forwarding setting as last seen, to act only when it changes.
    bool            m_portForwardingOn = false;

    VpnState m_currentState = VpnState::Unknown;
    QString  m_activeCity;
    // Country code of the connected server (e.g. "US") as `protonvpn status`
    // reported it; empty until it does, and again once a new connection
    // starts or the connection ends. See connectionCountry().
    QString  m_connectedCountryCode;
    // City from `protonvpn status`, for the Selected Location dropdown. Held
    // only until the state change that follows it, or until the cities list
    // has loaded; never past the connection it was reported for.
    QString  m_pendingStatusCity;
    QString  m_currentCityFeatures; // features string (e.g. "P2P") for the currently active city
    // The country and city m_currentCityFeatures was last asked for, so the
    // same server is not looked up twice (see fetchConnectedCityFeatures()).
    QString  m_cityFeaturesRequest;
    QString  m_lastConnectedInfo;   // base info text from the last Connected state change
    bool     m_hadUnknownConnection = false;
    bool     m_stateKnown = false;
    // nullopt = citiesReady has not yet fired for the local country;
    // empty list = fired but the CLI returned no cities.
    std::optional<QList<QPair<QString, QString>>> m_pendingCities;
    // Set once the Selected Location dropdown has its cities.
    bool     m_citiesPopulated = false;

    // Constructor helpers - each builds one self-contained part of the page.
    void buildPortRow(QWidget* parent, QVBoxLayout* layout);
    void buildBannerArea(QVBoxLayout* scrollLayout);
    void buildDrawer();

    void updateUi(VpnState state, const QString& info);
    void startElapsedTimer();
    void stopElapsedTimer() const;
    void showErrorDetails();
    void relayoutPickers() const; // delegates to drawer syncVisibility
    // The page's width less the quick settings panel, when it is shown. The
    // narrow and wide modes and the content scale go by this width.
    [[nodiscard]] int mainWidth() const;
    // Shows or hides the quick settings and picks the narrow or wide layout
    // for the page's size.
    void updateLayoutForSize();
    void applyWideMode(bool wide);
    void repositionDrawer();
    void repositionDrawerNotch(int drawerW);
    void updateDrawerNotchIcon();
    void checkPrereleaseBanner();
    void updateBannerAreaVisibility();
    // Fits the wide-mode warnings scroll area to the warnings in it.
    void updateBannerScrollHeight() const;
    // Scales the logo, power button, status text and their spacing with the
    // page size; 1 at the default window size (see vpnPage.cpp).
    void updateContentScale();
    qreal m_contentScale = 1.0;

    // Background globe; it faces connectionCountry() (see updateGlobeTarget()).
    GlobeWidget* m_globe = nullptr;
    void updateGlobeTarget();
    void syncInfoBackdrop() const;
    // Chooses between the narrow layout and the two wide arrangements: the
    // content spanning the full width above the expanded dropdowns, or
    // sitting in a column beside them.
    void updateArrangement();
    // Runs updateArrangement() once the pending layout pass is done.
    void scheduleArrangement();
    // Whether the content fits above the expanded dropdowns.
    [[nodiscard]] bool stackedArrangementFits() const;
    // The height the wide-mode sidebar takes below the content.
    [[nodiscard]] int stackedSidebarHeight() const;
    // In wide mode the sidebar holds the dropdowns and the warnings, and
    // shows when either has anything to show.
    void updatePickerSidebarVisibility() const;
    void applyFreeUserMode() const;
    void startNatPmpLoop();
    void stopNatPmpLoop();
    // Stops the keep-alive and hides the forwarded port, and nothing more:
    // stopNatPmpLoop() also forgets the connection, for a disconnect.
    void stopPortForwarding();
    void showNatpmpcBanner();
    void dismissNatpmpcBanner();
    void refreshConnectedInfoLabel() const;
    // Selects m_pendingStatusCity in the Selected Location dropdown once its
    // cities have loaded, if the connection is in the local country and the
    // city is listed; the dropdown is left as it is otherwise.
    void applyPendingStatusCity();
    // Populates the Selected Location dropdown and applies what was waiting
    // for its cities.
    void populateLocationPicker(const QList<QPair<QString, QString>>& cities);
    // Whether `countryCode` is the country the Selected Location dropdown
    // lists. False for every code when no local country was detected.
    [[nodiscard]] bool isLocalCountry(const QString& countryCode) const;
    // The country of the connection being made or in use: the one `protonvpn
    // status` reported, or until it has, the one that was asked for. Empty
    // when neither is known (the fastest server anywhere, before the first
    // status).
    [[nodiscard]] QString connectionCountry() const;
    // Forgets what `protonvpn status` said about a connection that has ended
    // or is being replaced.
    void forgetStatusLocation();
    // A dropdown row for a recent or favorite connection was chosen.
    void onSavedConnectionSelected(const QString& countryCode, const QString& city);
    // The Selected Location dropdown's selection changed while a connection
    // is up or being made.
    void onLocationSelectionChanged(const QString& city);
    // Asks whether to switch the connection to `locationName` now. With
    // `canDefer` the other choice is to keep the selection for the next
    // reconnect; without it, to leave the connection as it is. Returns true
    // for connecting now.
    [[nodiscard]] bool confirmLocationChange(const QString& locationName, bool canDefer);
    // Looks up whether the connected city's servers are P2P ones, for the
    // port forwarding note, once the connection's country and city are known.
    void fetchConnectedCityFeatures();
};
