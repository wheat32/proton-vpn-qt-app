#pragma once

// serverFilters.h
// The rules behind the Countries page's feature filters (P2P, Secure Core,
// Tor): which cities a set of filters lets through, and what a settings read
// does to the P2P filter. Lives in its own header, free of the page, so the
// rules can be unit tested.

#include <QLatin1Char>
#include <QLatin1String>
#include <QString>
#include <QStringList>
#include <algorithm>
#include "../uiHelpers.h"

namespace ServerFilters
{
// kServerFeatures' entries, by the filter each one is for.
constexpr int FEATURE_P2P         = 0;
constexpr int FEATURE_SECURE_CORE = 1;
constexpr int FEATURE_TOR         = 2;

// Whether a city's features ("P2P, Tor", as `protonvpn cities list` prints
// them) include `keyword`, in any letter case.
inline bool hasFeature(const QString& features, const char* keyword)
{
    const QStringList tags = features.split(QLatin1Char(','), Qt::SkipEmptyParts);
    return std::ranges::any_of(tags, [keyword](const QString& tag)
    {
        return tag.trimmed().contains(QLatin1String(keyword), Qt::CaseInsensitive);
    });
}

// Whether a city with `features` is listed under the filters that are on.
// Every filter that is on has to match; with none on, every city is listed.
inline bool cityPassesFilters(const QString& features,
                              const bool filterP2P,
                              const bool filterSecureCore,
                              const bool filterTor)
{
    if (filterP2P == true
        && hasFeature(features, kServerFeatures[FEATURE_P2P].keyword) == false)
    {
        return false;
    }

    if (filterSecureCore == true
        && hasFeature(features, kServerFeatures[FEATURE_SECURE_CORE].keyword) == false)
    {
        return false;
    }

    if (filterTor == true
        && hasFeature(features, kServerFeatures[FEATURE_TOR].keyword) == false)
    {
        return false;
    }

    return true;
}

// Whether the P2P filter is on after the settings were read.
//
// Port forwarding only works on P2P servers, so turning it on turns the
// filter on, and turning it off turns the filter off again. Only a change
// does either: a read that finds port forwarding as it was (every visit to
// the Settings page, every other setting changed) leaves alone a filter the
// user set by hand.
inline bool p2pFilterAfterSettingsRead(const bool wasForwarding, const bool isForwarding, const bool filterP2P)
{
    if (isForwarding == wasForwarding)
    {
        return filterP2P;
    }
    return isForwarding;
}
} // namespace ServerFilters
