#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>

// ---------------------------------------------------------------------------
// GeoUtils - shared utilities for country detection and flag icon loading.
// These are free functions in a namespace so any page can use them without
// coupling to a specific widget class.
// ---------------------------------------------------------------------------
namespace GeoUtils
{
    // Detect the user's country via system timezone then locale territory.
    // Returns the 2-letter uppercase code Proton VPN uses for it (e.g. "US",
    // "DE", and "UK" rather than "GB": see protonCountryCode()), or an empty
    // string if detection fails.
    QString detectUserCountry();

    // The code Proton VPN uses for the country with ISO 3166-1 alpha-2 code
    // `isoCode`. They are the same but for the United Kingdom, which Proton
    // calls "UK": the CLI rejects "GB" as an invalid country code, and its
    // server list names the country "UK". Any other code is returned as given.
    QString protonCountryCode(const QString& isoCode);

    // Convert a 2-letter country code to its English display name
    // (e.g. "US" -> "United States").  Returns the code itself if unknown.
    QString countryCodeToName(const QString& code);

    // Full name of a US state from its two-letter code ("NJ" -> "New Jersey").
    // Returns an empty string for an unknown code.
    QString usStateName(const QString& stateCode);

    // Full state name for a Proton VPN city, or an empty string when the city
    // is unknown or the country is not the US.  The CLI reports only a city
    // name, so US cities are resolved through a lookup table.
    QString regionForCity(const QString& countryCode, const QString& city);

    // "Secaucus" -> "Secaucus, New Jersey" for known US cities.  Any other
    // country - or an unknown US city - is returned unchanged, so callers can
    // use this for every country without special-casing.
    QString cityWithRegion(const QString& countryCode, const QString& city);

    // Inserts the US state into every Proton server string found in `text`:
    //   "Connected to US-NJ#203 in Secaucus, United States."
    //     -> "Connected to US-NJ#203 in Secaucus, New Jersey, United States."
    // The state comes from the server code when it carries one and from the
    // city table otherwise.  Text with no US server string is unchanged, and
    // running it over already-expanded text is a no-op.
    QString withUsStates(const QString& text);

    // Render an SVG resource path into a QPixmap at the given pixel size.
    QPixmap svgPixmap(const QString& resourcePath, int size = 16);

    // Render an SVG resource path into a QPixmap with explicit dimensions.
    // Useful for non-square assets such as 4:3 flags.
    QPixmap svgPixmap(const QString& resourcePath, int width, int height);

    // Same as above but color-tints the rendered pixmap.
    // Uses CompositionMode_SourceIn to replace color while preserving alpha.
    QPixmap svgPixmap(const QString& resourcePath, int size, const QColor& tint);
    QPixmap svgPixmap(const QString& resourcePath, int width, int height, const QColor& tint);

    // Overloads for Qt's named colors.
    //
    // These exist to close an overload-resolution trap: Qt::GlobalColor is an
    // enum, so svgPixmap(path, 80, Qt::white) would otherwise pick the
    // (path, width, height) overload - enum-to-int is a standard conversion and
    // beats the user-defined conversion to QColor - and silently render an
    // 80x3 pixmap (Qt::white == 3) instead of a white-tinted 80x80 one.
    QPixmap svgPixmap(const QString& resourcePath, int size, Qt::GlobalColor tint);
    QPixmap svgPixmap(const QString& resourcePath, int width, int height, Qt::GlobalColor tint);

    // Return a QIcon for the given country code using the embedded /flags/
    // resources (e.g. "US" -> :/flags/us).  Icons are cached so each SVG is
    // only decoded once per session.  Returns a null QIcon if no flag exists.
    QIcon flagIcon(const QString& countryCode);
} // namespace GeoUtils

