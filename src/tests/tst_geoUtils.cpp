#include <QtTest/QtTest>
#include "geoUtils.h"

class TstGeoUtils : public QObject
{
    Q_OBJECT

private slots:
    // countryCodeToName -------------------------------------------------------
    void countryCodeToName_knownCode_returnsName()
    {
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("US")), QStringLiteral("United States"));
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("DE")), QStringLiteral("Germany"));
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("CH")), QStringLiteral("Switzerland"));
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("JP")), QStringLiteral("Japan"));
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("GB")), QStringLiteral("United Kingdom"));
    }

    void countryCodeToName_lowercaseInput_returnsName()
    {
        // Input is normalised to upper-case internally.
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("us")), QStringLiteral("United States"));
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("fr")), QStringLiteral("France"));
    }

    void countryCodeToName_unknownCode_returnsCodeItself()
    {
        // The function documents that it returns the original code when unknown.
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("XX")), QStringLiteral("XX"));
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("ZZ")), QStringLiteral("ZZ"));
    }

    void countryCodeToName_emptyString_returnsEmptyString()
    {
        QCOMPARE(GeoUtils::countryCodeToName(QString()), QString());
    }

    void countryCodeToName_ukAlias_returnsSameAsGb()
    {
        // Both "GB" and "UK" are mapped to "United Kingdom".
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("UK")),
                 GeoUtils::countryCodeToName(QStringLiteral("GB")));
    }

    void protonCountryCode_unitedKingdom_isUk()
    {
        // The CLI rejects "GB" ("Invalid country code") and lists the country
        // as "UK", so that is what a UK user's own country has to be.
        QCOMPARE(GeoUtils::protonCountryCode(QStringLiteral("GB")), QStringLiteral("UK"));
        QCOMPARE(GeoUtils::protonCountryCode(QStringLiteral("gb")), QStringLiteral("UK"));
    }

    void protonCountryCode_otherCodes_passThrough()
    {
        QCOMPARE(GeoUtils::protonCountryCode(QStringLiteral("UK")), QStringLiteral("UK"));
        QCOMPARE(GeoUtils::protonCountryCode(QStringLiteral("US")), QStringLiteral("US"));
        QCOMPARE(GeoUtils::protonCountryCode(QStringLiteral("CH")), QStringLiteral("CH"));
        QVERIFY(GeoUtils::protonCountryCode(QString()).isEmpty());
    }

    void protonCountryCode_result_hasAFlagAndAName()
    {
        // The code handed to the CLI is also the one the UI looks up.
        const QString code = GeoUtils::protonCountryCode(QStringLiteral("GB"));
        QCOMPARE(GeoUtils::countryCodeToName(code), QStringLiteral("United Kingdom"));
        QVERIFY(QFile::exists(QStringLiteral(":/flags/") + code.toLower()));
    }

    // US states ---------------------------------------------------------------

    void usStateName_knownCode_returnsFullName()
    {
        QCOMPARE(GeoUtils::usStateName(QStringLiteral("NJ")), QStringLiteral("New Jersey"));
        QCOMPARE(GeoUtils::usStateName(QStringLiteral("nj")), QStringLiteral("New Jersey"));
        QCOMPARE(GeoUtils::usStateName(QStringLiteral("DC")), QStringLiteral("District of Columbia"));
    }

    void usStateName_unknownCode_returnsEmpty()
    {
        QVERIFY(GeoUtils::usStateName(QStringLiteral("ZZ")).isEmpty());
        QVERIFY(GeoUtils::usStateName(QString()).isEmpty());
    }

    void regionForCity_knownUsCity_returnsState()
    {
        QCOMPARE(GeoUtils::regionForCity(QStringLiteral("US"), QStringLiteral("Secaucus")),
                 QStringLiteral("New Jersey"));
        QCOMPARE(GeoUtils::regionForCity(QStringLiteral("US"), QStringLiteral("salt lake city")),
                 QStringLiteral("Utah"));
        // The CLI's "Washington" is the capital, not the state.
        QCOMPARE(GeoUtils::regionForCity(QStringLiteral("US"), QStringLiteral("Washington")),
                 QStringLiteral("District of Columbia"));
        QCOMPARE(GeoUtils::regionForCity(QStringLiteral("US"), QStringLiteral("Seattle")),
                 QStringLiteral("Washington"));
    }

    void regionForCity_nonUsCountry_returnsEmpty()
    {
        QVERIFY(GeoUtils::regionForCity(QStringLiteral("DE"), QStringLiteral("Frankfurt")).isEmpty());
        // A city that happens to share a US name must not pick up a US state.
        QVERIFY(GeoUtils::regionForCity(QStringLiteral("CA"), QStringLiteral("Boston")).isEmpty());
    }

    void regionForCity_unknownUsCity_returnsEmpty()
    {
        QVERIFY(GeoUtils::regionForCity(QStringLiteral("US"),
                                        QStringLiteral("Nowhereville")).isEmpty());
    }

    void cityWithRegion_knownUsCity_appendsState()
    {
        QCOMPARE(GeoUtils::cityWithRegion(QStringLiteral("US"), QStringLiteral("Secaucus")),
                 QStringLiteral("Secaucus, New Jersey"));
    }

    void cityWithRegion_otherCountries_areUnchanged()
    {
        QCOMPARE(GeoUtils::cityWithRegion(QStringLiteral("DE"), QStringLiteral("Frankfurt")),
                 QStringLiteral("Frankfurt"));
        QCOMPARE(GeoUtils::cityWithRegion(QStringLiteral("JP"), QStringLiteral("Tokyo")),
                 QStringLiteral("Tokyo"));
    }

    void cityWithRegion_cityEqualToItsState_isNotDuplicated()
    {
        // Seattle is in Washington; "Washington" (DC) must not become
        // "Washington, Washington" through either path.
        QCOMPARE(GeoUtils::cityWithRegion(QStringLiteral("US"), QStringLiteral("Washington")),
                 QStringLiteral("Washington, District of Columbia"));
    }

    void cityWithRegion_emptyCity_isUnchanged()
    {
        QVERIFY(GeoUtils::cityWithRegion(QStringLiteral("US"), QString()).isEmpty());
    }

    void withUsStates_serverWithStateCode_insertsState()
    {
        QCOMPARE(GeoUtils::withUsStates(
                     QStringLiteral("Connected to US-NJ#203 in Secaucus, United States.")),
                 QStringLiteral("Connected to US-NJ#203 in Secaucus, New Jersey, United States."));
    }

    void withUsStates_serverWithoutStateCode_usesCityTable()
    {
        QCOMPARE(GeoUtils::withUsStates(
                     QStringLiteral("Connected to US#12 in Denver, United States.")),
                 QStringLiteral("Connected to US#12 in Denver, Colorado, United States."));
    }

    void withUsStates_secureCoreServer_isHandled()
    {
        // Secure core exit servers read "<entry>-US#n".
        QCOMPARE(GeoUtils::withUsStates(
                     QStringLiteral("Connected to CH-US#1 in Secaucus, United States.")),
                 QStringLiteral("Connected to CH-US#1 in Secaucus, New Jersey, United States."));
    }

    void withUsStates_stateCodeWinsOverCityTable()
    {
        // The server name is authoritative when it carries a state code.
        QCOMPARE(GeoUtils::withUsStates(
                     QStringLiteral("US-TX#9 in Dallas, United States")),
                 QStringLiteral("US-TX#9 in Dallas, Texas, United States"));
    }

    void withUsStates_isIdempotent()
    {
        const QString expanded =
            QStringLiteral("Connected to US-NJ#203 in Secaucus, New Jersey, United States.");
        QCOMPARE(GeoUtils::withUsStates(expanded), expanded);
    }

    void withUsStates_nonUsServer_isUnchanged()
    {
        const QString german = QStringLiteral("Connected to DE#42 in Frankfurt, Germany.");
        QCOMPARE(GeoUtils::withUsStates(german), german);

        const QString swiss = QStringLiteral("Connected to CH#7 in Zurich, Switzerland.");
        QCOMPARE(GeoUtils::withUsStates(swiss), swiss);
    }

    void withUsStates_unknownCityAndNoStateCode_isUnchanged()
    {
        const QString text = QStringLiteral("Connected to US#5 in Nowhereville, United States.");
        QCOMPARE(GeoUtils::withUsStates(text), text);
    }

    void withUsStates_wordBoundary_doesNotMatchOtherCodes()
    {
        // "AUS#1" contains "US#1" but is not a US server.
        const QString text = QStringLiteral("Connected to AUS#1 in Sydney, Australia.");
        QCOMPARE(GeoUtils::withUsStates(text), text);
    }

    void withUsStates_multipleServersInOneText_allExpanded()
    {
        QCOMPARE(GeoUtils::withUsStates(
                     QStringLiteral("US-NJ#1 in Secaucus, United States / US-CA#2 in San Jose, United States")),
                 QStringLiteral("US-NJ#1 in Secaucus, New Jersey, United States / "
                                "US-CA#2 in San Jose, California, United States"));
    }

    void withUsStates_emptyText_returnsEmpty()
    {
        QVERIFY(GeoUtils::withUsStates(QString()).isEmpty());
    }

    // svgPixmap tint overloads ------------------------------------------------

    void svgPixmap_namedColorTint_keepsRequestedSize()
    {
        // Regression guard: Qt::GlobalColor is an enum, so a named color used
        // as a tint used to bind to the (width, height) overload instead -
        // Qt::white == 3 produced a 16x3 strip that callers then stretched.
        const QPixmap tinted =
            GeoUtils::svgPixmap(QStringLiteral(":/assets/power.svg"), 16, Qt::white);

        QCOMPARE(tinted.deviceIndependentSize(), QSizeF(16, 16));
    }

    void svgPixmap_namedColorTint_matchesQColorTint()
    {
        const QPixmap named =
            GeoUtils::svgPixmap(QStringLiteral(":/assets/power.svg"), 16, Qt::white);
        const QPixmap explicitColor =
            GeoUtils::svgPixmap(QStringLiteral(":/assets/power.svg"), 16, QColor(Qt::white));

        QCOMPARE(named.toImage(), explicitColor.toImage());
    }

    void svgPixmap_explicitDimensions_areHonored()
    {
        const QPixmap wide =
            GeoUtils::svgPixmap(QStringLiteral(":/assets/power.svg"), 20, 15);
        QCOMPARE(wide.deviceIndependentSize(), QSizeF(20, 15));
    }

    void svgPixmap_tintedIcon_isNotBlank()
    {
        // A tinted icon must still have opaque pixels: the overload mix-up
        // showed up as a 16x3 sliver stretched over the button.
        const QImage image =
            GeoUtils::svgPixmap(QStringLiteral(":/assets/power.svg"), 32,
                                QColor(Qt::white)).toImage();

        bool anyOpaque = false;
        for (int y = 0; y < image.height() && anyOpaque == false; ++y)
        {
            for (int x = 0; x < image.width(); ++x)
            {
                if (qAlpha(image.pixel(x, y)) > 0)
                {
                    anyOpaque = true;
                    break;
                }
            }
        }
        QVERIFY(anyOpaque);
    }
};

QTEST_MAIN(TstGeoUtils)
#include "tst_geoUtils.moc"

