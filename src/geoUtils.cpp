
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QMap>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QTimeZone>
#include "geoUtils.h"
#include "widgets/pixmapCache.h"

namespace GeoUtils
{

constexpr int ISO_COUNTRY_CODE_LENGTH = 2;
constexpr int FLAG_ICON_WIDTH = 20;
constexpr int FLAG_ICON_HEIGHT = 15;

// ---------------------------------------------------------------------------
// Timezone -> country-code mapping (representative/most-common zone per country)
// ---------------------------------------------------------------------------
static const QMap<QString, QString> kTimezoneToCountry = {
    // Americas
    {QStringLiteral("America/New_York"),        QStringLiteral("US")},
    {QStringLiteral("America/Chicago"),         QStringLiteral("US")},
    {QStringLiteral("America/Denver"),          QStringLiteral("US")},
    {QStringLiteral("America/Los_Angeles"),     QStringLiteral("US")},
    {QStringLiteral("America/Anchorage"),       QStringLiteral("US")},
    {QStringLiteral("Pacific/Honolulu"),        QStringLiteral("US")},
    {QStringLiteral("America/Phoenix"),         QStringLiteral("US")},
    {QStringLiteral("America/Toronto"),         QStringLiteral("CA")},
    {QStringLiteral("America/Vancouver"),       QStringLiteral("CA")},
    {QStringLiteral("America/Winnipeg"),        QStringLiteral("CA")},
    {QStringLiteral("America/Halifax"),         QStringLiteral("CA")},
    {QStringLiteral("America/St_Johns"),        QStringLiteral("CA")},
    {QStringLiteral("America/Mexico_City"),     QStringLiteral("MX")},
    {QStringLiteral("America/Sao_Paulo"),       QStringLiteral("BR")},
    {QStringLiteral("America/Buenos_Aires"),    QStringLiteral("AR")},
    {QStringLiteral("America/Santiago"),        QStringLiteral("CL")},
    {QStringLiteral("America/Lima"),            QStringLiteral("PE")},
    {QStringLiteral("America/Bogota"),          QStringLiteral("CO")},
    {QStringLiteral("America/Caracas"),         QStringLiteral("VE")},
    // Europe
    {QStringLiteral("Europe/London"),           QStringLiteral("GB")},
    {QStringLiteral("Europe/Paris"),            QStringLiteral("FR")},
    {QStringLiteral("Europe/Berlin"),           QStringLiteral("DE")},
    {QStringLiteral("Europe/Amsterdam"),        QStringLiteral("NL")},
    {QStringLiteral("Europe/Brussels"),         QStringLiteral("BE")},
    {QStringLiteral("Europe/Madrid"),           QStringLiteral("ES")},
    {QStringLiteral("Europe/Rome"),             QStringLiteral("IT")},
    {QStringLiteral("Europe/Lisbon"),           QStringLiteral("PT")},
    {QStringLiteral("Europe/Zurich"),           QStringLiteral("CH")},
    {QStringLiteral("Europe/Vienna"),           QStringLiteral("AT")},
    {QStringLiteral("Europe/Warsaw"),           QStringLiteral("PL")},
    {QStringLiteral("Europe/Prague"),           QStringLiteral("CZ")},
    {QStringLiteral("Europe/Budapest"),         QStringLiteral("HU")},
    {QStringLiteral("Europe/Bucharest"),        QStringLiteral("RO")},
    {QStringLiteral("Europe/Sofia"),            QStringLiteral("BG")},
    {QStringLiteral("Europe/Stockholm"),        QStringLiteral("SE")},
    {QStringLiteral("Europe/Oslo"),             QStringLiteral("NO")},
    {QStringLiteral("Europe/Copenhagen"),       QStringLiteral("DK")},
    {QStringLiteral("Europe/Helsinki"),         QStringLiteral("FI")},
    {QStringLiteral("Europe/Athens"),           QStringLiteral("GR")},
    {QStringLiteral("Europe/Kiev"),             QStringLiteral("UA")},
    {QStringLiteral("Europe/Kyiv"),             QStringLiteral("UA")},
    {QStringLiteral("Europe/Moscow"),           QStringLiteral("RU")},
    {QStringLiteral("Europe/Istanbul"),         QStringLiteral("TR")},
    {QStringLiteral("Europe/Riga"),             QStringLiteral("LV")},
    {QStringLiteral("Europe/Vilnius"),          QStringLiteral("LT")},
    {QStringLiteral("Europe/Tallinn"),          QStringLiteral("EE")},
    {QStringLiteral("Europe/Dublin"),           QStringLiteral("IE")},
    {QStringLiteral("Europe/Bratislava"),       QStringLiteral("SK")},
    {QStringLiteral("Europe/Ljubljana"),        QStringLiteral("SI")},
    {QStringLiteral("Europe/Zagreb"),           QStringLiteral("HR")},
    {QStringLiteral("Europe/Sarajevo"),         QStringLiteral("BA")},
    {QStringLiteral("Europe/Belgrade"),         QStringLiteral("RS")},
    {QStringLiteral("Europe/Skopje"),           QStringLiteral("MK")},
    {QStringLiteral("Europe/Podgorica"),        QStringLiteral("ME")},
    {QStringLiteral("Europe/Tirane"),           QStringLiteral("AL")},
    {QStringLiteral("Europe/Minsk"),            QStringLiteral("BY")},
    {QStringLiteral("Europe/Luxembourg"),       QStringLiteral("LU")},
    {QStringLiteral("Europe/Malta"),            QStringLiteral("MT")},
    {QStringLiteral("Europe/Nicosia"),          QStringLiteral("CY")},
    {QStringLiteral("Atlantic/Reykjavik"),      QStringLiteral("IS")},
    // Asia
    {QStringLiteral("Asia/Tokyo"),              QStringLiteral("JP")},
    {QStringLiteral("Asia/Seoul"),              QStringLiteral("KR")},
    {QStringLiteral("Asia/Shanghai"),           QStringLiteral("CN")},
    {QStringLiteral("Asia/Hong_Kong"),          QStringLiteral("HK")},
    {QStringLiteral("Asia/Singapore"),          QStringLiteral("SG")},
    {QStringLiteral("Asia/Bangkok"),            QStringLiteral("TH")},
    {QStringLiteral("Asia/Jakarta"),            QStringLiteral("ID")},
    {QStringLiteral("Asia/Manila"),             QStringLiteral("PH")},
    {QStringLiteral("Asia/Kuala_Lumpur"),       QStringLiteral("MY")},
    {QStringLiteral("Asia/Kolkata"),            QStringLiteral("IN")},
    {QStringLiteral("Asia/Karachi"),            QStringLiteral("PK")},
    {QStringLiteral("Asia/Dhaka"),              QStringLiteral("BD")},
    {QStringLiteral("Asia/Colombo"),            QStringLiteral("LK")},
    {QStringLiteral("Asia/Kathmandu"),          QStringLiteral("NP")},
    {QStringLiteral("Asia/Tashkent"),           QStringLiteral("UZ")},
    {QStringLiteral("Asia/Almaty"),             QStringLiteral("KZ")},
    {QStringLiteral("Asia/Tehran"),             QStringLiteral("IR")},
    {QStringLiteral("Asia/Baghdad"),            QStringLiteral("IQ")},
    {QStringLiteral("Asia/Riyadh"),             QStringLiteral("SA")},
    {QStringLiteral("Asia/Dubai"),              QStringLiteral("AE")},
    {QStringLiteral("Asia/Kuwait"),             QStringLiteral("KW")},
    {QStringLiteral("Asia/Qatar"),              QStringLiteral("QA")},
    {QStringLiteral("Asia/Beirut"),             QStringLiteral("LB")},
    {QStringLiteral("Asia/Damascus"),           QStringLiteral("SY")},
    {QStringLiteral("Asia/Amman"),              QStringLiteral("JO")},
    {QStringLiteral("Asia/Jerusalem"),          QStringLiteral("IL")},
    {QStringLiteral("Asia/Nicosia"),            QStringLiteral("CY")},
    {QStringLiteral("Asia/Taipei"),             QStringLiteral("TW")},
    {QStringLiteral("Asia/Ulaanbaatar"),        QStringLiteral("MN")},
    {QStringLiteral("Asia/Yerevan"),            QStringLiteral("AM")},
    {QStringLiteral("Asia/Tbilisi"),            QStringLiteral("GE")},
    {QStringLiteral("Asia/Baku"),               QStringLiteral("AZ")},
    // Oceania
    {QStringLiteral("Australia/Sydney"),        QStringLiteral("AU")},
    {QStringLiteral("Australia/Melbourne"),     QStringLiteral("AU")},
    {QStringLiteral("Australia/Brisbane"),      QStringLiteral("AU")},
    {QStringLiteral("Australia/Perth"),         QStringLiteral("AU")},
    {QStringLiteral("Australia/Adelaide"),      QStringLiteral("AU")},
    {QStringLiteral("Pacific/Auckland"),        QStringLiteral("NZ")},
    // Africa
    {QStringLiteral("Africa/Cairo"),            QStringLiteral("EG")},
    {QStringLiteral("Africa/Lagos"),            QStringLiteral("NG")},
    {QStringLiteral("Africa/Nairobi"),          QStringLiteral("KE")},
    {QStringLiteral("Africa/Johannesburg"),     QStringLiteral("ZA")},
    {QStringLiteral("Africa/Casablanca"),       QStringLiteral("MA")},
    {QStringLiteral("Africa/Algiers"),          QStringLiteral("DZ")},
    {QStringLiteral("Africa/Tunis"),            QStringLiteral("TN")},
    {QStringLiteral("Africa/Tripoli"),          QStringLiteral("LY")},
    {QStringLiteral("Africa/Accra"),            QStringLiteral("GH")},
    {QStringLiteral("Africa/Addis_Ababa"),      QStringLiteral("ET")},
};

// ---------------------------------------------------------------------------
// countryCodeToName
// ---------------------------------------------------------------------------
QString countryCodeToName(const QString& code)
{
    static const QMap<QString, const char*> kCodeToName = {
        {QStringLiteral("AF"), QT_TRANSLATE_NOOP("Country", "Afghanistan")},
        {QStringLiteral("AL"), QT_TRANSLATE_NOOP("Country", "Albania")},
        {QStringLiteral("DZ"), QT_TRANSLATE_NOOP("Country", "Algeria")},
        {QStringLiteral("AO"), QT_TRANSLATE_NOOP("Country", "Angola")},
        {QStringLiteral("AR"), QT_TRANSLATE_NOOP("Country", "Argentina")},
        {QStringLiteral("AM"), QT_TRANSLATE_NOOP("Country", "Armenia")},
        {QStringLiteral("AU"), QT_TRANSLATE_NOOP("Country", "Australia")},
        {QStringLiteral("AT"), QT_TRANSLATE_NOOP("Country", "Austria")},
        {QStringLiteral("AZ"), QT_TRANSLATE_NOOP("Country", "Azerbaijan")},
        {QStringLiteral("BH"), QT_TRANSLATE_NOOP("Country", "Bahrain")},
        {QStringLiteral("BD"), QT_TRANSLATE_NOOP("Country", "Bangladesh")},
        {QStringLiteral("BY"), QT_TRANSLATE_NOOP("Country", "Belarus")},
        {QStringLiteral("BE"), QT_TRANSLATE_NOOP("Country", "Belgium")},
        {QStringLiteral("BT"), QT_TRANSLATE_NOOP("Country", "Bhutan")},
        {QStringLiteral("BA"), QT_TRANSLATE_NOOP("Country", "Bosnia and Herzegovina")},
        {QStringLiteral("BR"), QT_TRANSLATE_NOOP("Country", "Brazil")},
        {QStringLiteral("BN"), QT_TRANSLATE_NOOP("Country", "Brunei")},
        {QStringLiteral("BG"), QT_TRANSLATE_NOOP("Country", "Bulgaria")},
        {QStringLiteral("KH"), QT_TRANSLATE_NOOP("Country", "Cambodia")},
        {QStringLiteral("CM"), QT_TRANSLATE_NOOP("Country", "Cameroon")},
        {QStringLiteral("CA"), QT_TRANSLATE_NOOP("Country", "Canada")},
        {QStringLiteral("TD"), QT_TRANSLATE_NOOP("Country", "Chad")},
        {QStringLiteral("CL"), QT_TRANSLATE_NOOP("Country", "Chile")},
        {QStringLiteral("CO"), QT_TRANSLATE_NOOP("Country", "Colombia")},
        {QStringLiteral("KM"), QT_TRANSLATE_NOOP("Country", "Comoros")},
        {QStringLiteral("CR"), QT_TRANSLATE_NOOP("Country", "Costa Rica")},
        {QStringLiteral("HR"), QT_TRANSLATE_NOOP("Country", "Croatia")},
        {QStringLiteral("CU"), QT_TRANSLATE_NOOP("Country", "Cuba")},
        {QStringLiteral("CY"), QT_TRANSLATE_NOOP("Country", "Cyprus")},
        {QStringLiteral("CZ"), QT_TRANSLATE_NOOP("Country", "Czech Republic")},
        {QStringLiteral("DK"), QT_TRANSLATE_NOOP("Country", "Denmark")},
        {QStringLiteral("DO"), QT_TRANSLATE_NOOP("Country", "Dominican Republic")},
        {QStringLiteral("EC"), QT_TRANSLATE_NOOP("Country", "Ecuador")},
        {QStringLiteral("EG"), QT_TRANSLATE_NOOP("Country", "Egypt")},
        {QStringLiteral("SV"), QT_TRANSLATE_NOOP("Country", "El Salvador")},
        {QStringLiteral("ER"), QT_TRANSLATE_NOOP("Country", "Eritrea")},
        {QStringLiteral("EE"), QT_TRANSLATE_NOOP("Country", "Estonia")},
        {QStringLiteral("ET"), QT_TRANSLATE_NOOP("Country", "Ethiopia")},
        {QStringLiteral("FI"), QT_TRANSLATE_NOOP("Country", "Finland")},
        {QStringLiteral("FR"), QT_TRANSLATE_NOOP("Country", "France")},
        {QStringLiteral("GE"), QT_TRANSLATE_NOOP("Country", "Georgia")},
        {QStringLiteral("DE"), QT_TRANSLATE_NOOP("Country", "Germany")},
        {QStringLiteral("GH"), QT_TRANSLATE_NOOP("Country", "Ghana")},
        {QStringLiteral("GR"), QT_TRANSLATE_NOOP("Country", "Greece")},
        {QStringLiteral("GT"), QT_TRANSLATE_NOOP("Country", "Guatemala")},
        {QStringLiteral("HN"), QT_TRANSLATE_NOOP("Country", "Honduras")},
        {QStringLiteral("HK"), QT_TRANSLATE_NOOP("Country", "Hong Kong")},
        {QStringLiteral("HU"), QT_TRANSLATE_NOOP("Country", "Hungary")},
        {QStringLiteral("IS"), QT_TRANSLATE_NOOP("Country", "Iceland")},
        {QStringLiteral("IN"), QT_TRANSLATE_NOOP("Country", "India")},
        {QStringLiteral("ID"), QT_TRANSLATE_NOOP("Country", "Indonesia")},
        {QStringLiteral("IQ"), QT_TRANSLATE_NOOP("Country", "Iraq")},
        {QStringLiteral("IE"), QT_TRANSLATE_NOOP("Country", "Ireland")},
        {QStringLiteral("IL"), QT_TRANSLATE_NOOP("Country", "Israel")},
        {QStringLiteral("IT"), QT_TRANSLATE_NOOP("Country", "Italy")},
        {QStringLiteral("CI"), QT_TRANSLATE_NOOP("Country", "Ivory Coast")},
        {QStringLiteral("JP"), QT_TRANSLATE_NOOP("Country", "Japan")},
        {QStringLiteral("JO"), QT_TRANSLATE_NOOP("Country", "Jordan")},
        {QStringLiteral("KZ"), QT_TRANSLATE_NOOP("Country", "Kazakhstan")},
        {QStringLiteral("KE"), QT_TRANSLATE_NOOP("Country", "Kenya")},
        {QStringLiteral("KW"), QT_TRANSLATE_NOOP("Country", "Kuwait")},
        {QStringLiteral("LA"), QT_TRANSLATE_NOOP("Country", "Laos")},
        {QStringLiteral("LV"), QT_TRANSLATE_NOOP("Country", "Latvia")},
        {QStringLiteral("LY"), QT_TRANSLATE_NOOP("Country", "Libya")},
        {QStringLiteral("LT"), QT_TRANSLATE_NOOP("Country", "Lithuania")},
        {QStringLiteral("LU"), QT_TRANSLATE_NOOP("Country", "Luxembourg")},
        {QStringLiteral("MK"), QT_TRANSLATE_NOOP("Country", "Macedonia")},
        {QStringLiteral("MY"), QT_TRANSLATE_NOOP("Country", "Malaysia")},
        {QStringLiteral("MT"), QT_TRANSLATE_NOOP("Country", "Malta")},
        {QStringLiteral("MR"), QT_TRANSLATE_NOOP("Country", "Mauritania")},
        {QStringLiteral("MU"), QT_TRANSLATE_NOOP("Country", "Mauritius")},
        {QStringLiteral("MX"), QT_TRANSLATE_NOOP("Country", "Mexico")},
        {QStringLiteral("MD"), QT_TRANSLATE_NOOP("Country", "Moldova")},
        {QStringLiteral("MN"), QT_TRANSLATE_NOOP("Country", "Mongolia")},
        {QStringLiteral("ME"), QT_TRANSLATE_NOOP("Country", "Montenegro")},
        {QStringLiteral("MA"), QT_TRANSLATE_NOOP("Country", "Morocco")},
        {QStringLiteral("MZ"), QT_TRANSLATE_NOOP("Country", "Mozambique")},
        {QStringLiteral("MM"), QT_TRANSLATE_NOOP("Country", "Myanmar")},
        {QStringLiteral("NP"), QT_TRANSLATE_NOOP("Country", "Nepal")},
        {QStringLiteral("NL"), QT_TRANSLATE_NOOP("Country", "Netherlands")},
        {QStringLiteral("NZ"), QT_TRANSLATE_NOOP("Country", "New Zealand")},
        {QStringLiteral("NG"), QT_TRANSLATE_NOOP("Country", "Nigeria")},
        {QStringLiteral("NO"), QT_TRANSLATE_NOOP("Country", "Norway")},
        {QStringLiteral("OM"), QT_TRANSLATE_NOOP("Country", "Oman")},
        {QStringLiteral("PK"), QT_TRANSLATE_NOOP("Country", "Pakistan")},
        {QStringLiteral("PS"), QT_TRANSLATE_NOOP("Country", "Palestinian Territory")},
        {QStringLiteral("PA"), QT_TRANSLATE_NOOP("Country", "Panama")},
        {QStringLiteral("PE"), QT_TRANSLATE_NOOP("Country", "Peru")},
        {QStringLiteral("PH"), QT_TRANSLATE_NOOP("Country", "Philippines")},
        {QStringLiteral("PL"), QT_TRANSLATE_NOOP("Country", "Poland")},
        {QStringLiteral("PT"), QT_TRANSLATE_NOOP("Country", "Portugal")},
        {QStringLiteral("PR"), QT_TRANSLATE_NOOP("Country", "Puerto Rico")},
        {QStringLiteral("QA"), QT_TRANSLATE_NOOP("Country", "Qatar")},
        {QStringLiteral("RO"), QT_TRANSLATE_NOOP("Country", "Romania")},
        {QStringLiteral("RU"), QT_TRANSLATE_NOOP("Country", "Russia")},
        {QStringLiteral("RW"), QT_TRANSLATE_NOOP("Country", "Rwanda")},
        {QStringLiteral("SA"), QT_TRANSLATE_NOOP("Country", "Saudi Arabia")},
        {QStringLiteral("SN"), QT_TRANSLATE_NOOP("Country", "Senegal")},
        {QStringLiteral("RS"), QT_TRANSLATE_NOOP("Country", "Serbia")},
        {QStringLiteral("SG"), QT_TRANSLATE_NOOP("Country", "Singapore")},
        {QStringLiteral("SK"), QT_TRANSLATE_NOOP("Country", "Slovakia")},
        {QStringLiteral("SI"), QT_TRANSLATE_NOOP("Country", "Slovenia")},
        {QStringLiteral("SO"), QT_TRANSLATE_NOOP("Country", "Somalia")},
        {QStringLiteral("ZA"), QT_TRANSLATE_NOOP("Country", "South Africa")},
        {QStringLiteral("KR"), QT_TRANSLATE_NOOP("Country", "South Korea")},
        {QStringLiteral("SS"), QT_TRANSLATE_NOOP("Country", "South Sudan")},
        {QStringLiteral("ES"), QT_TRANSLATE_NOOP("Country", "Spain")},
        {QStringLiteral("LK"), QT_TRANSLATE_NOOP("Country", "Sri Lanka")},
        {QStringLiteral("SD"), QT_TRANSLATE_NOOP("Country", "Sudan")},
        {QStringLiteral("SE"), QT_TRANSLATE_NOOP("Country", "Sweden")},
        {QStringLiteral("CH"), QT_TRANSLATE_NOOP("Country", "Switzerland")},
        {QStringLiteral("SY"), QT_TRANSLATE_NOOP("Country", "Syria")},
        {QStringLiteral("TW"), QT_TRANSLATE_NOOP("Country", "Taiwan")},
        {QStringLiteral("TJ"), QT_TRANSLATE_NOOP("Country", "Tajikistan")},
        {QStringLiteral("TZ"), QT_TRANSLATE_NOOP("Country", "Tanzania")},
        {QStringLiteral("TH"), QT_TRANSLATE_NOOP("Country", "Thailand")},
        {QStringLiteral("TG"), QT_TRANSLATE_NOOP("Country", "Togo")},
        {QStringLiteral("TN"), QT_TRANSLATE_NOOP("Country", "Tunisia")},
        {QStringLiteral("TR"), QT_TRANSLATE_NOOP("Country", "Turkey")},
        {QStringLiteral("TM"), QT_TRANSLATE_NOOP("Country", "Turkmenistan")},
        {QStringLiteral("UG"), QT_TRANSLATE_NOOP("Country", "Uganda")},
        {QStringLiteral("UA"), QT_TRANSLATE_NOOP("Country", "Ukraine")},
        {QStringLiteral("AE"), QT_TRANSLATE_NOOP("Country", "United Arab Emirates")},
        {QStringLiteral("GB"), QT_TRANSLATE_NOOP("Country", "United Kingdom")},
        {QStringLiteral("UK"), QT_TRANSLATE_NOOP("Country", "United Kingdom")},
        {QStringLiteral("US"), QT_TRANSLATE_NOOP("Country", "United States")},
        {QStringLiteral("UZ"), QT_TRANSLATE_NOOP("Country", "Uzbekistan")},
        {QStringLiteral("VE"), QT_TRANSLATE_NOOP("Country", "Venezuela")},
        {QStringLiteral("VN"), QT_TRANSLATE_NOOP("Country", "Vietnam")},
        {QStringLiteral("YE"), QT_TRANSLATE_NOOP("Country", "Yemen")},
    };
    const auto it = kCodeToName.find(code.toUpper());
    // Names are stored as QT_TRANSLATE_NOOP source strings so lupdate can pick
    // them up; they are translated here at the point of use.  Falling back to
    // the raw code keeps unknown codes visible rather than blank.
    return it != kCodeToName.end()
           ? QCoreApplication::translate("Country", it.value())
           : code;
}

// ---------------------------------------------------------------------------
// US states
//
// `protonvpn cities list US` reports only a city name, and the status line only
// "<City>, United States" - which is ambiguous for anyone who thinks in states.
// These two tables let the UI show the state as well.
// ---------------------------------------------------------------------------

namespace
{
// Two-letter state code -> full name. Names are QT_TRANSLATE_NOOP source
// strings so lupdate collects them, matching how country names are handled.
const QMap<QString, const char*>& usStateNames()
{
    static const QMap<QString, const char*> kNames = {
        {QStringLiteral("AL"), QT_TRANSLATE_NOOP("UsState", "Alabama")},
        {QStringLiteral("AK"), QT_TRANSLATE_NOOP("UsState", "Alaska")},
        {QStringLiteral("AZ"), QT_TRANSLATE_NOOP("UsState", "Arizona")},
        {QStringLiteral("AR"), QT_TRANSLATE_NOOP("UsState", "Arkansas")},
        {QStringLiteral("CA"), QT_TRANSLATE_NOOP("UsState", "California")},
        {QStringLiteral("CO"), QT_TRANSLATE_NOOP("UsState", "Colorado")},
        {QStringLiteral("CT"), QT_TRANSLATE_NOOP("UsState", "Connecticut")},
        {QStringLiteral("DE"), QT_TRANSLATE_NOOP("UsState", "Delaware")},
        {QStringLiteral("DC"), QT_TRANSLATE_NOOP("UsState", "District of Columbia")},
        {QStringLiteral("FL"), QT_TRANSLATE_NOOP("UsState", "Florida")},
        {QStringLiteral("GA"), QT_TRANSLATE_NOOP("UsState", "Georgia")},
        {QStringLiteral("HI"), QT_TRANSLATE_NOOP("UsState", "Hawaii")},
        {QStringLiteral("ID"), QT_TRANSLATE_NOOP("UsState", "Idaho")},
        {QStringLiteral("IL"), QT_TRANSLATE_NOOP("UsState", "Illinois")},
        {QStringLiteral("IN"), QT_TRANSLATE_NOOP("UsState", "Indiana")},
        {QStringLiteral("IA"), QT_TRANSLATE_NOOP("UsState", "Iowa")},
        {QStringLiteral("KS"), QT_TRANSLATE_NOOP("UsState", "Kansas")},
        {QStringLiteral("KY"), QT_TRANSLATE_NOOP("UsState", "Kentucky")},
        {QStringLiteral("LA"), QT_TRANSLATE_NOOP("UsState", "Louisiana")},
        {QStringLiteral("ME"), QT_TRANSLATE_NOOP("UsState", "Maine")},
        {QStringLiteral("MD"), QT_TRANSLATE_NOOP("UsState", "Maryland")},
        {QStringLiteral("MA"), QT_TRANSLATE_NOOP("UsState", "Massachusetts")},
        {QStringLiteral("MI"), QT_TRANSLATE_NOOP("UsState", "Michigan")},
        {QStringLiteral("MN"), QT_TRANSLATE_NOOP("UsState", "Minnesota")},
        {QStringLiteral("MS"), QT_TRANSLATE_NOOP("UsState", "Mississippi")},
        {QStringLiteral("MO"), QT_TRANSLATE_NOOP("UsState", "Missouri")},
        {QStringLiteral("MT"), QT_TRANSLATE_NOOP("UsState", "Montana")},
        {QStringLiteral("NE"), QT_TRANSLATE_NOOP("UsState", "Nebraska")},
        {QStringLiteral("NV"), QT_TRANSLATE_NOOP("UsState", "Nevada")},
        {QStringLiteral("NH"), QT_TRANSLATE_NOOP("UsState", "New Hampshire")},
        {QStringLiteral("NJ"), QT_TRANSLATE_NOOP("UsState", "New Jersey")},
        {QStringLiteral("NM"), QT_TRANSLATE_NOOP("UsState", "New Mexico")},
        {QStringLiteral("NY"), QT_TRANSLATE_NOOP("UsState", "New York")},
        {QStringLiteral("NC"), QT_TRANSLATE_NOOP("UsState", "North Carolina")},
        {QStringLiteral("ND"), QT_TRANSLATE_NOOP("UsState", "North Dakota")},
        {QStringLiteral("OH"), QT_TRANSLATE_NOOP("UsState", "Ohio")},
        {QStringLiteral("OK"), QT_TRANSLATE_NOOP("UsState", "Oklahoma")},
        {QStringLiteral("OR"), QT_TRANSLATE_NOOP("UsState", "Oregon")},
        {QStringLiteral("PA"), QT_TRANSLATE_NOOP("UsState", "Pennsylvania")},
        {QStringLiteral("RI"), QT_TRANSLATE_NOOP("UsState", "Rhode Island")},
        {QStringLiteral("SC"), QT_TRANSLATE_NOOP("UsState", "South Carolina")},
        {QStringLiteral("SD"), QT_TRANSLATE_NOOP("UsState", "South Dakota")},
        {QStringLiteral("TN"), QT_TRANSLATE_NOOP("UsState", "Tennessee")},
        {QStringLiteral("TX"), QT_TRANSLATE_NOOP("UsState", "Texas")},
        {QStringLiteral("UT"), QT_TRANSLATE_NOOP("UsState", "Utah")},
        {QStringLiteral("VT"), QT_TRANSLATE_NOOP("UsState", "Vermont")},
        {QStringLiteral("VA"), QT_TRANSLATE_NOOP("UsState", "Virginia")},
        {QStringLiteral("WA"), QT_TRANSLATE_NOOP("UsState", "Washington")},
        {QStringLiteral("WV"), QT_TRANSLATE_NOOP("UsState", "West Virginia")},
        {QStringLiteral("WI"), QT_TRANSLATE_NOOP("UsState", "Wisconsin")},
        {QStringLiteral("WY"), QT_TRANSLATE_NOOP("UsState", "Wyoming")},
        {QStringLiteral("PR"), QT_TRANSLATE_NOOP("UsState", "Puerto Rico")},
    };
    return kNames;
}

// Lowercased Proton VPN city name -> state code.
//
// The server name usually carries the state already ("US-NJ#203"), so this
// table is only consulted for city lists and for server names without one.
// Cities missing from the table simply render without a state rather than
// guessing - deliberately, since some US city names are ambiguous across
// states and a wrong state is worse than none.
const QMap<QString, QString>& usCityStates()
{
    static const QMap<QString, QString> kCities = {
        {QStringLiteral("albuquerque"), QStringLiteral("NM")},
        {QStringLiteral("anchorage"), QStringLiteral("AK")},
        {QStringLiteral("ashburn"), QStringLiteral("VA")},
        {QStringLiteral("atlanta"), QStringLiteral("GA")},
        {QStringLiteral("austin"), QStringLiteral("TX")},
        {QStringLiteral("baltimore"), QStringLiteral("MD")},
        {QStringLiteral("boise"), QStringLiteral("ID")},
        {QStringLiteral("boston"), QStringLiteral("MA")},
        {QStringLiteral("buffalo"), QStringLiteral("NY")},
        {QStringLiteral("charlotte"), QStringLiteral("NC")},
        {QStringLiteral("cheyenne"), QStringLiteral("WY")},
        {QStringLiteral("chicago"), QStringLiteral("IL")},
        {QStringLiteral("cincinnati"), QStringLiteral("OH")},
        {QStringLiteral("cleveland"), QStringLiteral("OH")},
        {QStringLiteral("columbus"), QStringLiteral("OH")},
        {QStringLiteral("coppell"), QStringLiteral("TX")},
        {QStringLiteral("dallas"), QStringLiteral("TX")},
        {QStringLiteral("denver"), QStringLiteral("CO")},
        {QStringLiteral("detroit"), QStringLiteral("MI")},
        {QStringLiteral("elk grove village"), QStringLiteral("IL")},
        {QStringLiteral("fremont"), QStringLiteral("CA")},
        {QStringLiteral("hartford"), QStringLiteral("CT")},
        {QStringLiteral("honolulu"), QStringLiteral("HI")},
        {QStringLiteral("houston"), QStringLiteral("TX")},
        {QStringLiteral("indianapolis"), QStringLiteral("IN")},
        {QStringLiteral("jacksonville"), QStringLiteral("FL")},
        {QStringLiteral("kansas city"), QStringLiteral("MO")},
        {QStringLiteral("las vegas"), QStringLiteral("NV")},
        {QStringLiteral("los angeles"), QStringLiteral("CA")},
        {QStringLiteral("louisville"), QStringLiteral("KY")},
        {QStringLiteral("manassas"), QStringLiteral("VA")},
        {QStringLiteral("mcallen"), QStringLiteral("TX")},
        {QStringLiteral("memphis"), QStringLiteral("TN")},
        {QStringLiteral("miami"), QStringLiteral("FL")},
        {QStringLiteral("milwaukee"), QStringLiteral("WI")},
        {QStringLiteral("minneapolis"), QStringLiteral("MN")},
        {QStringLiteral("nashville"), QStringLiteral("TN")},
        {QStringLiteral("new orleans"), QStringLiteral("LA")},
        {QStringLiteral("new york"), QStringLiteral("NY")},
        {QStringLiteral("newark"), QStringLiteral("NJ")},
        {QStringLiteral("oklahoma city"), QStringLiteral("OK")},
        {QStringLiteral("omaha"), QStringLiteral("NE")},
        {QStringLiteral("orlando"), QStringLiteral("FL")},
        {QStringLiteral("philadelphia"), QStringLiteral("PA")},
        {QStringLiteral("phoenix"), QStringLiteral("AZ")},
        {QStringLiteral("piscataway"), QStringLiteral("NJ")},
        {QStringLiteral("pittsburgh"), QStringLiteral("PA")},
        {QStringLiteral("portland"), QStringLiteral("OR")},
        {QStringLiteral("providence"), QStringLiteral("RI")},
        {QStringLiteral("raleigh"), QStringLiteral("NC")},
        {QStringLiteral("richmond"), QStringLiteral("VA")},
        {QStringLiteral("sacramento"), QStringLiteral("CA")},
        {QStringLiteral("salt lake city"), QStringLiteral("UT")},
        {QStringLiteral("san antonio"), QStringLiteral("TX")},
        {QStringLiteral("san diego"), QStringLiteral("CA")},
        {QStringLiteral("san francisco"), QStringLiteral("CA")},
        {QStringLiteral("san jose"), QStringLiteral("CA")},
        {QStringLiteral("seattle"), QStringLiteral("WA")},
        {QStringLiteral("secaucus"), QStringLiteral("NJ")},
        {QStringLiteral("st. louis"), QStringLiteral("MO")},
        {QStringLiteral("tampa"), QStringLiteral("FL")},
        {QStringLiteral("washington"), QStringLiteral("DC")},
    };
    return kCities;
}

bool isUnitedStates(const QString& countryCode)
{
    const QString code = countryCode.toUpper();
    return code == QLatin1String("US") || code == QLatin1String("USA");
}
} // namespace

QString usStateName(const QString& stateCode)
{
    const auto it = usStateNames().find(stateCode.toUpper());
    return it != usStateNames().end()
           ? QCoreApplication::translate("UsState", it.value())
           : QString();
}

QString regionForCity(const QString& countryCode, const QString& city)
{
    if (isUnitedStates(countryCode) == false || city.isEmpty())
    {
        return {};
    }

    const auto it = usCityStates().find(city.trimmed().toLower());
    return it != usCityStates().end() ? usStateName(it.value()) : QString();
}

QString cityWithRegion(const QString& countryCode, const QString& city)
{
    const QString region = regionForCity(countryCode, city);
    if (region.isEmpty() || city.contains(region, Qt::CaseInsensitive))
    {
        return city;
    }

    return city + QStringLiteral(", ") + region;
}

QString withUsStates(const QString& text)
{
    // Matches the server strings the CLI prints, with or without a state code:
    //   "US-NJ#203 in Secaucus, United States"
    //   "US#12 in Secaucus, United States"      (also secure-core "CH-US#1 ...")
    // The trailing ", United States" keeps the match from firing on anything
    // that merely happens to contain "US#".
    static const QRegularExpression serverRe(
        QStringLiteral(R"(\bUS(?:-([A-Z]{2}))?#\d+\s+in\s+([^,\n]+?)\s*,\s*United States)"));

    QString result;
    qsizetype copiedTo = 0;

    QRegularExpressionMatchIterator it = serverRe.globalMatch(text);
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        const QString stateCode = match.captured(1);
        const QString city      = match.captured(2);

        const QString state = stateCode.isEmpty()
            ? regionForCity(QStringLiteral("US"), city)
            : usStateName(stateCode);

        // Unknown state, or the city is the state (Washington): leave it alone.
        if (state.isEmpty() || city.compare(state, Qt::CaseInsensitive) == 0) continue;

        const qsizetype cityEnd = match.capturedStart(2) + match.capturedLength(2);
        result += text.mid(copiedTo, cityEnd - copiedTo);
        result += QStringLiteral(", ") + state;
        copiedTo = cityEnd;
    }

    result += text.mid(copiedTo);
    return result;
}

// ---------------------------------------------------------------------------
// protonCountryCode
// ---------------------------------------------------------------------------
QString protonCountryCode(const QString& isoCode)
{
    if (isoCode.compare(QLatin1String("GB"), Qt::CaseInsensitive) == 0)
    {
        return QStringLiteral("UK");
    }
    return isoCode;
}

// ---------------------------------------------------------------------------
// detectUserCountry
// ---------------------------------------------------------------------------
QString detectUserCountry()
{
    // Both sources give ISO codes, and every caller hands the result to the
    // CLI or compares it with the CLI's country list, so each is returned as
    // Proton's code.

    // 1. Try system timezone
    const QByteArray tzId = QTimeZone::systemTimeZoneId();
    if (tzId.isEmpty() == false)
    {
        const QString tzStr = QString::fromUtf8(tzId);
        const auto it = kTimezoneToCountry.find(tzStr);
        if (it != kTimezoneToCountry.end())
        {
            return protonCountryCode(it.value());
        }
    }

    // 2. Try QLocale territory
    const QLocale sysLocale = QLocale::system();
    const QLocale::Territory territory = sysLocale.territory();
    if (territory != QLocale::AnyTerritory)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
        const QString code = QLocale::territoryToCode(territory);
        if (code.isEmpty() == false)
        {
            return protonCountryCode(code.toUpper());
        }
#endif
        // Fallback: parse the BCP-47 name (e.g. "en-US" -> "US")
        const QString bcp47 = sysLocale.bcp47Name();
        const qsizetype dashPos = bcp47.indexOf(QLatin1Char('-'));
        if (dashPos != -1)
        {
            const QString regionTag = bcp47.mid(dashPos + 1).toUpper();
            if (regionTag.length() == ISO_COUNTRY_CODE_LENGTH && regionTag[0].isLetter() && regionTag[1].isLetter())
            {
                return protonCountryCode(regionTag);
            }
        }
    }

    return {};
}

// ---------------------------------------------------------------------------
// svgPixmap
// ---------------------------------------------------------------------------
QPixmap svgPixmap(const QString& resourcePath, int width, int height)
{
    // Render at the screen's device pixel ratio and tag the pixmap with it.
    // Rendering at the logical size and letting the compositor upscale is what
    // made every flag, nav icon and tray icon look soft on HiDPI displays.
    const qreal dpr = (qApp != nullptr) ? qApp->devicePixelRatio() : 1.0;

    QSvgRenderer renderer(resourcePath);
    QPixmap pixmap = blankPixmap(QSizeF(width, height), dpr);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    return pixmap;
}

QPixmap svgPixmap(const QString& resourcePath, int size)
{
    return svgPixmap(resourcePath, size, size);
}

QPixmap svgPixmap(const QString& resourcePath, int width, int height, const QColor& tint)
{
    QPixmap pixmap = svgPixmap(resourcePath, width, height);
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    // Painter coordinates are logical (the pixmap carries a device pixel
    // ratio), so fill the device-independent area rather than pixmap.rect().
    painter.fillRect(QRectF(QPointF(0, 0), pixmap.deviceIndependentSize()), tint);
    return pixmap;
}

QPixmap svgPixmap(const QString& resourcePath, int size, const QColor& tint)
{
    return svgPixmap(resourcePath, size, size, tint);
}

QPixmap svgPixmap(const QString& resourcePath, int size, Qt::GlobalColor tint)
{
    return svgPixmap(resourcePath, size, size, QColor(tint));
}

QPixmap svgPixmap(const QString& resourcePath, int width, int height, Qt::GlobalColor tint)
{
    return svgPixmap(resourcePath, width, height, QColor(tint));
}

// ---------------------------------------------------------------------------
// flagIcon
// ---------------------------------------------------------------------------
QIcon flagIcon(const QString& countryCode)
{
    static QMap<QString, QIcon> cache;
    const QString key = countryCode.toLower();
    const auto it = cache.find(key);
    if (it != cache.end())
    {
        return it.value();
    }

    const QString path = QStringLiteral(":/flags/") + key;
    if (QFile::exists(path) == false)
    {
        cache.insert(key, QIcon());
        return {};
    }

    // Render flags at 4:3 so they keep their natural aspect ratio.
    const QIcon icon(svgPixmap(path, FLAG_ICON_WIDTH, FLAG_ICON_HEIGHT));
    cache.insert(key, icon);
    return icon;
}

} // namespace GeoUtils

