#pragma once

// serverList.h
// Reads the server list the protonvpn CLI keeps in its cache
// (~/.cache/Proton/VPN/serverlist.json): every server's name, location, plan
// tier, features, load and addresses. The CLI's commands report far less
// (`cities list` names a city's features and nothing else), so the Properties
// window takes its details from here. The file is the CLI's own and not
// documented: a server missing its name is skipped, and any other missing
// field is left at its default. Lives in its own header so the parsing can be
// unit tested.

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QList>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>

namespace ServerList
{
// The flags in a server's "Features".
enum Feature : int
{
    SecureCore = 1 << 0,
    Tor        = 1 << 1,
    P2P        = 1 << 2,
    Streaming  = 1 << 3,
    IPv6       = 1 << 4,
};

// The plan a server needs, from its "Tier".
enum Tier : int
{
    FreeTier  = 0,
    BasicTier = 1,
    PlusTier  = 2,
};

// One of the physical servers (the "nodes") behind a logical server.
struct Node
{
    QString domain;        // "node-us-1.protonvpn.net"
    QString entryIp;
    QString exitIp;
    QString label;         // usually empty
    bool online = false;
};

// One server as the CLI lists it (a "logical" server): the physical servers
// behind it share its name and location, and their addresses are collected.
struct Server
{
    QString name;          // "US-NJ#203"; Secure Core names their entry too ("SE-JP#1")
    QString entryCountry;  // where traffic enters: another country for Secure Core
    QString exitCountry;   // where it leaves: the country the server is listed under
    QString city;
    int tier = FreeTier;
    int features = 0;      // Feature flags
    int load = 0;          // percent
    double score = 0.0;    // lower is faster
    bool online = false;
    double latitude = 0.0;
    double longitude = 0.0;
    QStringList entryIps;  // each node's, without repeats
    QStringList exitIps;
    QStringList domains;
    QList<Node> nodes;
};

namespace Detail
{
inline void appendUnique(QStringList& list, const QString& value)
{
    if (value.isEmpty() == false && list.contains(value) == false)
    {
        list << value;
    }
}

// A boolean in the CLI's cache, a number in the API it comes from.
inline bool isOnline(const QJsonValue& status)
{
    return status.isBool() == true ? status.toBool() : status.toInt() != 0;
}
} // namespace Detail

// The servers in the cache file's JSON; empty for anything else. When the
// JSON itself is broken, as in a file read while it was being written,
// `error` gets the parser's reason; it is left empty for valid JSON.
inline QList<Server> parse(const QByteArray& json, QString* error = nullptr)
{
    if (error != nullptr)
    {
        error->clear();
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("%1 at byte %2 of %3")
                         .arg(parseError.errorString()).arg(parseError.offset).arg(json.size());
        }
        return {};
    }

    QList<Server> servers;
    const QJsonArray logicals = document.object().value(QStringLiteral("LogicalServers")).toArray();
    servers.reserve(logicals.size());
    for (const QJsonValue& value : logicals)
    {
        const QJsonObject obj = value.toObject();
        Server server;
        server.name = obj.value(QStringLiteral("Name")).toString();
        if (server.name.isEmpty() == true) continue;
        server.entryCountry = obj.value(QStringLiteral("EntryCountry")).toString();
        server.exitCountry  = obj.value(QStringLiteral("ExitCountry")).toString();
        server.city         = obj.value(QStringLiteral("City")).toString();
        server.tier         = obj.value(QStringLiteral("Tier")).toInt(FreeTier);
        server.features     = obj.value(QStringLiteral("Features")).toInt();
        server.load         = obj.value(QStringLiteral("Load")).toInt();
        server.score        = obj.value(QStringLiteral("Score")).toDouble();
        server.online = Detail::isOnline(obj.value(QStringLiteral("Status")));
        const QJsonObject location = obj.value(QStringLiteral("ExitLocation")).toObject();
        server.latitude  = location.value(QStringLiteral("Latitude")).toDouble();
        server.longitude = location.value(QStringLiteral("Longitude")).toDouble();
        for (const QJsonValue& physical : obj.value(QStringLiteral("Servers")).toArray())
        {
            const QJsonObject phys = physical.toObject();
            Node node;
            node.domain  = phys.value(QStringLiteral("Domain")).toString();
            node.entryIp = phys.value(QStringLiteral("EntryIP")).toString();
            node.exitIp  = phys.value(QStringLiteral("ExitIP")).toString();
            node.label   = phys.value(QStringLiteral("Label")).toString();
            node.online  = Detail::isOnline(phys.value(QStringLiteral("Status")));
            Detail::appendUnique(server.entryIps, node.entryIp);
            Detail::appendUnique(server.exitIps, node.exitIp);
            Detail::appendUnique(server.domains, node.domain);
            server.nodes.append(node);
        }
        servers.append(server);
    }
    return servers;
}

// The servers of a location: those listed under the country, and when a
// city is given, in that city. Both compare without regard to case.
inline QList<Server> serversAt(const QList<Server>& servers, const QString& countryCode, const QString& city)
{
    QList<Server> found;
    for (const Server& server : servers)
    {
        if (server.exitCountry.compare(countryCode, Qt::CaseInsensitive) != 0) continue;
        if (city.isEmpty() == false && server.city.compare(city, Qt::CaseInsensitive) != 0) continue;
        found.append(server);
    }
    return found;
}

// Names for a server's feature flags, in a fixed order.
inline QStringList featureNames(const int features)
{
    QStringList names;
    for (const auto& [flag, name] : std::initializer_list<std::pair<Feature, const char*>>{
             {SecureCore, QT_TRANSLATE_NOOP("ServerList", "Secure Core")},
             {Tor,        QT_TRANSLATE_NOOP("ServerList", "Tor")},
             {P2P,        QT_TRANSLATE_NOOP("ServerList", "P2P")},
             {Streaming,  QT_TRANSLATE_NOOP("ServerList", "Streaming")},
             {IPv6,       QT_TRANSLATE_NOOP("ServerList", "IPv6")}})
    {
        if ((features & flag) != 0)
        {
            names << QCoreApplication::translate("ServerList", name);
        }
    }
    return names;
}

// The plan a server needs, by name.
inline QString tierName(const int tier)
{
    switch (tier)
    {
        case FreeTier:
            return QCoreApplication::translate("ServerList", "Free");
        case BasicTier:
            return QCoreApplication::translate("ServerList", "Basic");
        case PlusTier:
            return QCoreApplication::translate("ServerList", "Plus");
        default:
            return QString::number(tier);
    }
}

// Counts and loads for a group of servers. The loads are the online
// servers' only; with none online they are all zero.
struct Summary
{
    int count = 0;
    int online = 0;
    int free = 0;
    int minLoad = 0;
    int maxLoad = 0;
    int averageLoad = 0;
};

inline Summary summarize(const QList<Server>& servers)
{
    Summary summary;
    summary.count = static_cast<int>(servers.size());
    int loadTotal = 0;
    for (const Server& server : servers)
    {
        if (server.tier == FreeTier)
        {
            ++summary.free;
        }
        if (server.online == false) continue;
        summary.minLoad = summary.online == 0 ? server.load : std::min(summary.minLoad, server.load);
        summary.maxLoad = std::max(summary.maxLoad, server.load);
        loadTotal += server.load;
        ++summary.online;
    }
    if (summary.online > 0)
    {
        summary.averageLoad = static_cast<int>(std::lround(static_cast<double>(loadTotal) / summary.online));
    }
    return summary;
}
// A server in one line, for verbose output: name, city and country (and
// where a Secure Core server's traffic enters), load, plan, features,
// status and how many nodes it has.
inline QString describe(const Server& server)
{
    QString country = server.exitCountry;
    if (server.entryCountry.isEmpty() == false && server.entryCountry != server.exitCountry)
    {
        country += QStringLiteral(" via ") + server.entryCountry;
    }
    const QStringList features = featureNames(server.features);
    const qsizetype nodeCount = server.nodes.size();
    return QStringLiteral("%1: %2, %3, load %4%, %5, %6, %7, %8")
        .arg(server.name, server.city.isEmpty() == true ? QStringLiteral("no city") : server.city, country)
        .arg(server.load)
        .arg(tierName(server.tier),
             features.isEmpty() == true ? QStringLiteral("no features") : features.join(QStringLiteral(", ")),
             server.online == true ? QStringLiteral("online") : QStringLiteral("offline"),
             nodeCount == 1 ? QStringLiteral("1 node") : QStringLiteral("%1 nodes").arg(nodeCount));
}

// A node in one line, for verbose output: domain, addresses, label and status.
inline QString describe(const Node& node)
{
    QString text = QStringLiteral("%1: entry %2, exit %3")
                       .arg(node.domain.isEmpty() == true ? QStringLiteral("no domain") : node.domain,
                            node.entryIp, node.exitIp);
    if (node.label.isEmpty() == false)
    {
        text += QStringLiteral(", label ") + node.label;
    }
    return text + (node.online == true ? QStringLiteral(", online") : QStringLiteral(", offline"));
}
} // namespace ServerList
