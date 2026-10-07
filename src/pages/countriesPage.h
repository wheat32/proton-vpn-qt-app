#pragma once

#include <functional>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
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
    explicit CountriesPage(VpnManager* manager, QWidget* parent = nullptr);
    void refresh();
    // Set while the page sits beside the home page (MainWindow's split view):
    // it is then styled and spaced like the home page's quick settings, with
    // the countries as a grid of cards above their cities.
    void setSplitView(bool split);

signals:
    void connectRequested(const QString& countryCode, const QString& city);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    VpnManager* m_manager;
    QString     m_localCountryCode;

    QMap<QString, QString> m_allCountries; // name -> code
    QMap<QString, QList<QPair<QString, QString>>> m_cityCache; // code -> cities
    QSet<QString> m_pendingCityCodes;

    QString m_selectedCode;
    QString m_selectedCity;
    QString m_selectedCountry;

    static constexpr int kNarrowThreshold = 515;
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

    // Narrow layout
    QWidget*     m_narrowWidget      = nullptr;
    QScrollArea* m_narrowScroll      = nullptr;
    QWidget*     m_narrowContent     = nullptr;
    QVBoxLayout* m_narrowLayout      = nullptr;
    QLabel*      m_narrowLoadingLabel = nullptr;
    QPushButton* m_narrowConnectBtn  = nullptr;

    struct AccordionItem {
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

    void applyFilter();
    void updateBubbleStyles() const;
    bool countryPassesFilter(const QString& code) const;

    void populateWide() const;
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
    void populateNarrow();

    void addWideCityItem(const QString& city, const QString& features) const;
    void addNarrowCityItem(QVBoxLayout* layout, const QString& city,
                           const QString& features, const QString& code);

    void onCountriesReady(const QMap<QString, QString>& countries);
    void onCitiesReady(const QString& code, const QList<QPair<QString, QString>>& cities);

    void onWideCountrySelected(const QListWidgetItem* item);
    void onWideCitySelected(const QListWidgetItem* item);

    void toggleAccordion(const QString& code);
    void ensureCities(const QString& code);

    // Updates both connect buttons to show a lock when the account is Free,
    // or restores normal styling for paid accounts.
    void updateConnectBtnLockState() const;

    // Shows the "Disable Port Forwarding?" confirmation dialog.
    // bodyText is the message shown; onConfirm is called if the user confirms.
    void showDisablePortForwardingDialog(const QString& bodyText,
                                         const std::function<void()>& onConfirm);

    static QString bubbleStyle(bool active);
};
