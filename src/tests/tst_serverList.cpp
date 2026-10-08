#include <QtTest/QtTest>
#include "cli/serverList.h"

namespace
{
// The shape of the CLI's cache, cut down to a few servers: a plain one with
// two physical servers, a Secure Core one entering elsewhere, an offline free
// one whose status is a number (as the API sends it), and one with no name.
const QByteArray SAMPLE = R"({
    "LogicalServers": [
        {
            "Name": "US-NJ#203", "EntryCountry": "US", "ExitCountry": "US", "City": "Secaucus",
            "Tier": 2, "Features": 4, "Load": 40, "Score": 1.5, "Status": true,
            "ExitLocation": {"Latitude": 40.79, "Longitude": -74.06},
            "Servers": [
                {"EntryIP": "203.0.113.1", "ExitIP": "203.0.113.2", "Domain": "node-us-1.example", "Status": 1},
                {"EntryIP": "203.0.113.1", "ExitIP": "203.0.113.3", "Domain": "node-us-2.example", "Status": 0,
                 "Label": "4"}
            ]
        },
        {
            "Name": "CH-US#1", "EntryCountry": "CH", "ExitCountry": "US", "City": "secaucus",
            "Tier": 2, "Features": 1, "Load": 80, "Status": true, "Servers": []
        },
        {
            "Name": "US-FREE#5", "EntryCountry": "US", "ExitCountry": "US", "City": "Chicago",
            "Tier": 0, "Features": 0, "Load": 99, "Status": 0, "Servers": []
        },
        {
            "EntryCountry": "US", "ExitCountry": "US", "City": "Chicago", "Tier": 2
        },
        {
            "Name": "DE#10", "EntryCountry": "DE", "ExitCountry": "DE", "City": "Berlin",
            "Tier": 2, "Features": 18, "Load": 10, "Status": 1, "Servers": []
        }
    ]
})";
} // namespace

class TstServerList : public QObject
{
    Q_OBJECT

private slots:
    void parse_readsEveryNamedServer()
    {
        const QList<ServerList::Server> servers = ServerList::parse(SAMPLE);
        QCOMPARE(servers.size(), 4);
    }

    void parse_readsFields()
    {
        const ServerList::Server server = ServerList::parse(SAMPLE).first();
        QCOMPARE(server.name, QStringLiteral("US-NJ#203"));
        QCOMPARE(server.exitCountry, QStringLiteral("US"));
        QCOMPARE(server.city, QStringLiteral("Secaucus"));
        QCOMPARE(server.tier, static_cast<int>(ServerList::PlusTier));
        QCOMPARE(server.features, static_cast<int>(ServerList::P2P));
        QCOMPARE(server.load, 40);
        QCOMPARE(server.online, true);
        QCOMPARE(server.latitude, 40.79);
    }

    void parse_collectsDistinctAddresses()
    {
        const ServerList::Server server = ServerList::parse(SAMPLE).first();
        QCOMPARE(server.entryIps, QStringList({QStringLiteral("203.0.113.1")}));
        QCOMPARE(server.exitIps, QStringList({QStringLiteral("203.0.113.2"), QStringLiteral("203.0.113.3")}));
        QCOMPARE(server.domains.size(), 2);
    }

    void parse_numericStatus_readsOnline()
    {
        const QList<ServerList::Server> servers = ServerList::parse(SAMPLE);
        QCOMPARE(servers.at(2).online, false);
        QCOMPARE(servers.at(3).online, true);
    }

    void parse_notTheCache_returnsNothing()
    {
        QVERIFY(ServerList::parse("not json").isEmpty());
        QVERIFY(ServerList::parse(R"({"Code": 1000})").isEmpty());
    }

    void parse_readsNodes()
    {
        const QList<ServerList::Node> nodes = ServerList::parse(SAMPLE).first().nodes;
        QCOMPARE(nodes.size(), 2);
        QCOMPARE(nodes.at(0).domain, QStringLiteral("node-us-1.example"));
        QCOMPARE(nodes.at(0).exitIp, QStringLiteral("203.0.113.2"));
        QCOMPARE(nodes.at(0).online, true);
        QCOMPARE(nodes.at(1).online, false);
        QCOMPARE(nodes.at(1).label, QStringLiteral("4"));
    }

    void parse_halfWrittenFile_reportsTheError()
    {
        // As read while the CLI was writing it out.
        QString error;
        QVERIFY(ServerList::parse(SAMPLE.left(SAMPLE.size() / 2), &error).isEmpty());
        QVERIFY(error.isEmpty() == false);
    }

    void parse_validJson_leavesTheErrorEmpty()
    {
        QString error = QStringLiteral("stale");
        QVERIFY(ServerList::parse(SAMPLE, &error).isEmpty() == false);
        QVERIFY(error.isEmpty());
        // Valid, just not the cache: no servers, and no error either.
        QVERIFY(ServerList::parse(R"({"Code": 1000})", &error).isEmpty());
        QVERIFY(error.isEmpty());
    }

    void describe_server_namesItsDetails()
    {
        const QList<ServerList::Server> servers = ServerList::parse(SAMPLE);
        QCOMPARE(ServerList::describe(servers.at(0)),
                 QStringLiteral("US-NJ#203: Secaucus, US, load 40%, Plus, P2P, online, 2 nodes"));
        QCOMPARE(ServerList::describe(servers.at(1)),
                 QStringLiteral("CH-US#1: secaucus, US via CH, load 80%, Plus, Secure Core, online, 0 nodes"));
        QCOMPARE(ServerList::describe(servers.at(2)),
                 QStringLiteral("US-FREE#5: Chicago, US, load 99%, Free, no features, offline, 0 nodes"));
    }

    void describe_node_namesItsAddresses()
    {
        const QList<ServerList::Node> nodes = ServerList::parse(SAMPLE).first().nodes;
        QCOMPARE(ServerList::describe(nodes.at(0)),
                 QStringLiteral("node-us-1.example: entry 203.0.113.1, exit 203.0.113.2, online"));
        QCOMPARE(ServerList::describe(nodes.at(1)),
                 QStringLiteral("node-us-2.example: entry 203.0.113.1, exit 203.0.113.3, label 4, offline"));
    }

    void serversAt_city_matchesWithoutCase()
    {
        const QList<ServerList::Server> found =
            ServerList::serversAt(ServerList::parse(SAMPLE), QStringLiteral("us"), QStringLiteral("SECAUCUS"));
        QCOMPARE(found.size(), 2);
        QCOMPARE(found.at(1).name, QStringLiteral("CH-US#1"));
    }

    void serversAt_noCity_isTheWholeCountry()
    {
        const QList<ServerList::Server> found =
            ServerList::serversAt(ServerList::parse(SAMPLE), QStringLiteral("US"), QString());
        QCOMPARE(found.size(), 3);
    }

    void featureNames_listsSetFlags()
    {
        QCOMPARE(ServerList::featureNames(ServerList::Streaming | ServerList::IPv6),
                 QStringList({QStringLiteral("Streaming"), QStringLiteral("IPv6")}));
        QVERIFY(ServerList::featureNames(0).isEmpty());
    }

    void tierName_namesPlans()
    {
        QCOMPARE(ServerList::tierName(ServerList::FreeTier), QStringLiteral("Free"));
        QCOMPARE(ServerList::tierName(ServerList::PlusTier), QStringLiteral("Plus"));
    }

    void summarize_countsAndOnlineLoads()
    {
        const ServerList::Summary summary = ServerList::summarize(
            ServerList::serversAt(ServerList::parse(SAMPLE), QStringLiteral("US"), QString()));
        QCOMPARE(summary.count, 3);
        QCOMPARE(summary.online, 2);
        QCOMPARE(summary.free, 1);
        // The offline server's 99% is left out.
        QCOMPARE(summary.minLoad, 40);
        QCOMPARE(summary.maxLoad, 80);
        QCOMPARE(summary.averageLoad, 60);
    }

    void summarize_noneOnline_loadsAreZero()
    {
        const ServerList::Summary summary = ServerList::summarize(
            ServerList::serversAt(ServerList::parse(SAMPLE), QStringLiteral("US"), QStringLiteral("Chicago")));
        QCOMPARE(summary.count, 1);
        QCOMPARE(summary.online, 0);
        QCOMPARE(summary.averageLoad, 0);
    }
};

QTEST_MAIN(TstServerList)
#include "tst_serverList.moc"
