#pragma once

// cliSettings.h
// Reads the VPN settings the CLI reports through `protonvpn config list`, and
// formats the custom DNS list for `protonvpn config set custom-dns`. Lives in
// its own header so the parsing can be unit tested.
//
// `config list` is the source of truth rather than the CLI's settings.json:
// the CLI only writes that file once a setting is changed and uses defaults
// the file does not show until then (IPv6, VPN Accelerator, and crash reports
// on; NetShield blocking malware on paid plans), and the Flatpak cannot see
// the file at all if the CLI created its folder after the app started.

#include <QLatin1Char>
#include <QMap>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include "cliTable.h"

namespace CliSettings
{
namespace Detail
{
// What the CLI puts after the custom DNS servers it shows when there are more.
inline const QString TRUNCATION_MARK = QStringLiteral("...");
} // namespace Detail

// Parses the table `protonvpn config list` prints:
//
//   Setting                  Value
//   -----------------------  ------------
//   netshield                malware-only
//   custom-dns               on  [1.1.1.1, 8.8.8.8]
//
// into setting -> value, e.g. "netshield" -> "malware-only". Empty when the
// output has no table, for example because the CLI printed an error instead.
inline QMap<QString, QString> parseConfigList(const QString& output)
{
    QMap<QString, QString> settings;
    for (const QString& row : CliTable::rows(output))
    {
        // Setting names have no spaces, and the value is everything after the
        // name: custom DNS pads its own value ("on  [1.1.1.1]"), so splitting
        // the row into cells would cut it apart.
        const qsizetype gap = row.indexOf(QLatin1Char(' '));
        if (gap < 0) continue;
        settings.insert(row.left(gap), row.mid(gap).trimmed());
    }
    return settings;
}

// Whether `config list` ended with its note on upgrading to VPN Plus, which
// the CLI prints for a free account only.
inline bool offersPlusUpgrade(const QString& configListOutput)
{
    return configListOutput.contains(QStringLiteral("To upgrade to VPN Plus"), Qt::CaseInsensitive);
}

// The servers in a custom DNS value from `config list`:
// "on  [1.1.1.1, 8.8.8.8]" -> {"1.1.1.1", "8.8.8.8"}. The CLI shows only the
// first few, followed by "...", so `truncated` (when given) tells the caller
// whether the list is complete.
inline QStringList customDnsServers(const QString& value, bool* truncated = nullptr)
{
    QStringList servers;
    bool cut = false;
    const qsizetype open = value.indexOf(QLatin1Char('['));
    const qsizetype close = value.lastIndexOf(QLatin1Char(']'));
    if (open >= 0 && close > open)
    {
        for (const QString& part : value.mid(open + 1, close - open - 1).split(QLatin1Char(',')))
        {
            const QString server = part.trimmed();
            if (server == Detail::TRUNCATION_MARK)
            {
                cut = true;
            }
            else if (server.isEmpty() == false)
            {
                servers << server;
            }
        }
    }
    if (truncated != nullptr)
    {
        *truncated = cut;
    }
    return servers;
}

// The custom DNS setting in the form the Settings page shows it, once it is
// known to be on: the servers, comma-separated, or just "on" without any.
inline QString customDnsValue(const QStringList& servers)
{
    return servers.isEmpty() == true ? QStringLiteral("on") : servers.join(QLatin1Char(','));
}

// Turns what the user typed in the custom DNS field into the comma-separated
// list `config set custom-dns --dns` expects, accepting commas, spaces, or
// both between addresses: "1.1.1.1, 8.8.8.8" -> "1.1.1.1,8.8.8.8". Without
// this, a space after a comma split the list into separate arguments, which
// the CLI rejected.
inline QString normalizeDnsList(const QString& text)
{
    static const QRegularExpression separators(QStringLiteral("[,\\s]+"));
    return text.split(separators, Qt::SkipEmptyParts).join(QLatin1Char(','));
}
} // namespace CliSettings
