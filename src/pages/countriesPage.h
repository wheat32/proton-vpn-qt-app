#pragma once

#include <functional>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QPushButton>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include "../vpnManager.h"
#include "../widgets/elidaLabel.h"

class GripSplitter;
class QSplitter;

// ---------------------------------------------------------------------------
// CountriesPage – server browser with feature-filter bubbles and a
// responsive layout (wide = side-by-side splitter, narrow = accordion list).
// ---------------------------------------------------------------------------
class CountriesPage : public QWidget
{
    Q_OBJECT

public:
    // Connect: the Countries page, whose buttons connect. Choose: the same
    // list in a dialog (the auto-connect server chooser), whose buttons pick
    // the location instead (see locationChosen()), for any account.
    enum class Mode
    {
        Connect,
        Choose,
    };

    explicit CountriesPage(VpnManager* manager, QWidget* parent = nullptr, Mode mode = Mode::Connect);
    void refresh();
    // Choose mode: shows the countries the main Countries page last fetched
    // (fetching them when it has not yet), with `countryCode` and `city`
    // selected, or the local country when `countryCode` is empty. For a page
    // that is on screen, so it has its layout for its width.
    void startChoosing(const QString& countryCode, const QString& city);
    // Set while the page sits beside the home page (MainWindow's split view):
    // it is then styled and spaced like the home page's quick settings, with
    // the countries as a grid of cards above their cities.
    void setSplitView(bool split);

signals:
    void connectRequested(const QString& countryCode, const QString& city);
    // Choose mode's counterpart of connectRequested(); `city` is empty for
    // the country's fastest server.
    void locationChosen(const QString& countryCode, const QString& city);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    // A country's cities as the CLI lists them: (city, features).
    using CityList = QList<QPair<QString, QString>>;
    // The countries a search and the filters leave: (name, code).
    using CountryList = QList<QPair<QString, QString>>;

    // What the countries list holds: its loading message, the countries, or
    // the message that they could not be loaded.
    enum class ListState
    {
        Loading,
        Ready,
        Failed,
    };

    static constexpr int NARROW_THRESHOLD = 515;

    VpnManager* m_manager;
    Mode        m_mode;
    QString     m_localCountryCode;
    // Selected once the countries, and then the country's cities, are in
    // (see startChoosing()).
    QString     m_initialCode;
    QString     m_initialCity;

    ListState m_countriesState = ListState::Loading;
    QMap<QString, QString> m_allCountries; // name -> code
    QMap<QString, CityList> m_cityCache;   // code -> cities
    QSet<QString> m_pendingCityCodes;

    // The one selection both layouts and both connect buttons show (see
    // updateConnectButtons()). The city is empty for the country's fastest
    // server.
    QString m_selectedCode;
    QString m_selectedCity;
    QString m_selectedCountry;
    // The selected country is not listed right now (the search or the filters
    // leave it out); it is selected again once it is.
    bool    m_selectionHidden = false;

    bool m_narrowMode = false;
    bool m_splitView  = false; // see setSplitView()

    bool m_filterP2P        = false;
    bool m_filterSecureCore = false;
    bool m_filterTor        = false;
    bool m_portForwardingEnabled = false;
    bool m_isFreeUser        = false;

    QLineEdit*   m_searchEdit     = nullptr;
    QPushButton* m_refreshBtn     = nullptr;
    QLabel*      m_countriesLabel = nullptr;

    QPushButton* m_bubbleAll        = nullptr;
    QPushButton* m_bubbleP2P        = nullptr;
    QPushButton* m_bubbleSecureCore = nullptr;
    QPushButton* m_bubbleTor        = nullptr;

    // Wide layout. The two panes sit side by side in m_listSplitter, or in
    // the split view one above the other in m_stackSplitter.
    QWidget*     m_wideWidget    = nullptr;
    QSplitter*   m_listSplitter  = nullptr;
    GripSplitter* m_stackSplitter = nullptr;
    QWidget*     m_countriesPane = nullptr;
    QWidget*     m_citiesPane    = nullptr;
    QListWidget* m_countriesList = nullptr;
    QListWidget* m_citiesList    = nullptr;
    ElideLabel*  m_citiesLabel   = nullptr;
    QPushButton* m_connectBtn    = nullptr;

    // Narrow layout. m_narrowStatusLabel (the loading and failure message)
    // lives for as long as the page does and is only shown and hidden;
    // m_narrowLayout holds nothing but the accordion's sections, which come
    // and go with every rebuild.
    QWidget*     m_narrowWidget      = nullptr;
    QWidget*     m_narrowContent     = nullptr;
    QLabel*      m_narrowStatusLabel = nullptr;
    QVBoxLayout* m_narrowLayout      = nullptr;
    QPushButton* m_narrowConnectBtn  = nullptr;

    struct AccordionItem
    {
        QToolButton* headerBtn    = nullptr;
        QWidget*     citiesWidget = nullptr;
        QVBoxLayout* citiesLayout = nullptr;
        bool         expanded     = false;
    };
    QMap<QString, AccordionItem> m_accordion;

    QTimer* m_countriesSpinnerTimer = nullptr;
    QTimer* m_spinnerTimer          = nullptr;
    int m_countriesSpinnerFrame = 0;
    int m_spinnerFrame          = 0;

    void buildWideLayout(QVBoxLayout* parent);
    void buildNarrowLayout(QVBoxLayout* parent);
    void switchLayout(bool narrow);

    // Loading state. Both layouts are put into it, and taken out of it, in
    // one place each, so neither can be left behind.
    void showCountriesLoading();
    void showCountriesFailed();
    [[nodiscard]] QString loadingCountriesText() const;
    [[nodiscard]] QString loadingCitiesText() const;

    void applyFilter();
    void updateBubbleStyles() const;
    [[nodiscard]] bool anyFilterActive() const;
    // The local country, listed first and marked with a star.
    [[nodiscard]] bool isPinned(const QString& code) const;
    [[nodiscard]] bool countryPassesFilter(const QString& code) const;
    // The countries the search and the filters leave: the local country
    // first, then the rest by name.
    [[nodiscard]] CountryList matchingCountries() const;
    // The cities the feature filters leave.
    [[nodiscard]] CityList filteredCities(const CityList& cities) const;
    // Follows port forwarding, which needs P2P servers (see the .cpp).
    void onSettingsRead(const QMap<QString, QString>& settings);

    void populateWide(const CountryList& countries) const;
    // Shows the countries as a grid of cards in the split view, as a list
    // otherwise; see the .cpp.
    void updateCountriesListMode() const;
    // Sizes the grid's cards so whole cards fill each row.
    void updateCountryGridSize() const;
    // Puts the two panes of the split view back to their default sizes.
    void resetStackSizes() const;
    // Redraws the accordion's arrows, which follow the theme (as do the
    // filter bubbles, see bubbleStyle()).
    void refreshAccordionIcons() const;
    void populateNarrow(const CountryList& countries);
    // Takes every section out of the accordion.
    void clearAccordion();

    // The wide cities pane. Emptying it also stops its loading spinner, so
    // the spinner never writes over a row that is not its own.
    void clearWideCities() const;
    // Shows the selected country's cities, fetching them when needed.
    void showWideCities();
    void fillWideCities(const CityList& cities);
    void addWideCityItem(const QString& city, const QString& features) const;

    // An accordion section's rows: the cached cities the filters leave, or a
    // single message.
    void fillAccordionCities(const QString& code);
    void showAccordionMessage(const QString& code, const QString& text);
    void addNarrowCityItem(QVBoxLayout* layout, const QString& city,
                           const QString& features, const QString& code);

    void onCountriesReady(const QMap<QString, QString>& countries);
    void onCountriesFetchFailed();
    void onCitiesReady(const QString& code, const CityList& cities);
    void onCitiesFetchFailed(const QString& code);

    void onWideCountrySelected(const QListWidgetItem* item);
    void onWideCitySelected(const QListWidgetItem* item);

    void toggleAccordion(const QString& code);
    void ensureCities(const QString& code);

    // Whether this account may pick a location to connect to. Picking one in
    // choose mode connects to nothing, so that is for any account.
    [[nodiscard]] bool isLocked() const;
    [[nodiscard]] QString lockedTooltip() const;
    // Shows the selection on both connect buttons, the one in the layout on
    // screen and the one that is not: their text, whether they can be
    // clicked, and for a Free account the lock in place of both.
    void updateConnectButtons() const;
    // An accordion row's button, which a Free account cannot use either.
    void updateRowButtonLock(QPushButton* button) const;
    // What both connect buttons do: the selection they show.
    void onConnectClicked();

    // Connects to the location, or in choose mode picks it.
    void requestLocation(const QString& countryCode, const QString& city);
    // The connect buttons' text for a selected country, or a city in it.
    [[nodiscard]] QString actionText(const QString& country, const QString& city = QString()) const;
    // And with nothing selected.
    [[nodiscard]] QString noSelectionText() const;

    // Shows the "Disable Port Forwarding?" confirmation dialog.
    // bodyText is the message shown; onConfirm is called if the user confirms.
    void showDisablePortForwardingDialog(const QString& bodyText,
                                         const std::function<void()>& onConfirm);

    static QString bubbleStyle(bool active);
};
