#include "startupServerDialog.h"
#include "../appConfig.h"
#include "../geoUtils.h"
#include "../pages/countriesPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
constexpr int DIALOG_WIDTH      = 820;
constexpr int DIALOG_HEIGHT     = 640;
// Wide enough that the Countries list keeps its two panes side by side.
constexpr int DIALOG_MIN_WIDTH  = 640;
constexpr int DIALOG_MIN_HEIGHT = 480;
// The list brings its own 16 px margins; the text above it and the buttons
// below it line up with them.
constexpr int SIDE_MARGIN       = 16;
constexpr int TOP_MARGIN        = 16;
constexpr int BOTTOM_MARGIN     = 16;
constexpr int BUTTON_SPACING    = 8;
} // namespace

StartupServerDialog::StartupServerDialog(VpnManager* manager, const QString& serverKey, QWidget* parent)
    : QDialog(parent), m_serverKey(serverKey)
{
    setWindowTitle(tr("Auto-connect Server"));
    setModal(true);
    setMinimumSize(DIALOG_MIN_WIDTH, DIALOG_MIN_HEIGHT);
    resize(DIALOG_WIDTH, DIALOG_HEIGHT);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, TOP_MARGIN, 0, BOTTOM_MARGIN);
    layout->setSpacing(0);

    QLabel* intro = new QLabel(
        tr("When the app starts, it connects to <b>%1</b>. Choose a country or a city below, or the "
           "fastest server in any country.").arg(displayName(serverKey).toHtmlEscaped()),
        this);
    intro->setTextFormat(Qt::RichText);
    intro->setWordWrap(true);
    intro->setContentsMargins(SIDE_MARGIN, 0, SIDE_MARGIN, 0);
    layout->addWidget(intro);

    CountriesPage* countries = new CountriesPage(manager, this, CountriesPage::Mode::Choose);
    connect(countries, &CountriesPage::locationChosen, this, [this](const QString& code, const QString& city)
    {
        m_serverKey = AppConfig::autoConnectServerKey({.countryCode = code, .city = city});
        accept();
    });
    layout->addWidget(countries, 1);

    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->setContentsMargins(SIDE_MARGIN, 0, SIDE_MARGIN, 0);
    buttons->setSpacing(BUTTON_SPACING);
    QPushButton* fastestButton = new QPushButton(tr("Fastest Server"), this);
    fastestButton->setObjectName(QStringLiteral("secondaryButton"));
    fastestButton->setCursor(Qt::PointingHandCursor);
    fastestButton->setToolTip(tr("Connect to the fastest server in any country."));
    connect(fastestButton, &QPushButton::clicked, this, [this]()
    {
        m_serverKey.clear();
        accept();
    });
    buttons->addWidget(fastestButton);
    buttons->addStretch();
    QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);
    cancelButton->setObjectName(QStringLiteral("secondaryButton"));
    cancelButton->setCursor(Qt::PointingHandCursor);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(cancelButton);
    layout->addLayout(buttons);

    // Once the dialog is up: until then the list is too narrow for its two
    // panes, and would select the location in its one-column layout instead.
    const AppConfig::ServerLocation location = AppConfig::autoConnectServerLocation(serverKey);
    QTimer::singleShot(0, countries, [countries, location]()
    {
        countries->startChoosing(location.countryCode, location.city);
    });
}

// static
QString StartupServerDialog::displayName(const QString& serverKey)
{
    const AppConfig::ServerLocation location = AppConfig::autoConnectServerLocation(serverKey);
    if (location.countryCode.isEmpty() == true)
    {
        return tr("Fastest Server");
    }
    const QString country = GeoUtils::countryCodeToName(location.countryCode);
    if (location.city.isEmpty() == true)
    {
        return tr("Fastest in %1").arg(country);
    }
    return tr("%1, %2").arg(country, GeoUtils::cityWithRegion(location.countryCode, location.city));
}

// static
QIcon StartupServerDialog::icon(const QString& serverKey)
{
    const QString countryCode = AppConfig::autoConnectServerLocation(serverKey).countryCode;
    return countryCode.isEmpty() == true ? QIcon() : GeoUtils::flagIcon(countryCode);
}
