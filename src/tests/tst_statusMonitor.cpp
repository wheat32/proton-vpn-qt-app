#include <QtTest/QtTest>
#include "cli/statusMonitor.h"

// Tests cover the public static parsing helpers:
//   StatusMonitor::parseStatusFields()
//   StatusMonitor::parseCityFromServer()
//   StatusMonitor::parseServerName()
//   StatusMonitor::parseCountryFromServer()
// These are pure-function text parsers that drive all VPN state decisions in
// the app, making them critical to correctness.

class TstStatusMonitor : public QObject
{
    Q_OBJECT

private slots:

    //  parseStatusFields

    void parseStatusFields_connectedOutput_returnsStatusAndServer()
    {
        const QString input =
            QStringLiteral("Status:  Connected\n"
                           "Server:  US-NJ#203 in Secaucus, United States\n"
                           "Country: United States\n"
                           "IP:      192.0.2.1\n");

        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);

        QCOMPARE(fields.value(QStringLiteral("status")),  QStringLiteral("Connected"));
        QCOMPARE(fields.value(QStringLiteral("server")),
                 QStringLiteral("US-NJ#203 in Secaucus, United States"));
        QCOMPARE(fields.value(QStringLiteral("country")), QStringLiteral("United States"));
        QCOMPARE(fields.value(QStringLiteral("ip")),      QStringLiteral("192.0.2.1"));
    }

    void parseStatusFields_disconnectedOutput_returnsDisconnectedStatus()
    {
        const QString input = QStringLiteral("Status:  Disconnected\n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QCOMPARE(fields.value(QStringLiteral("status")), QStringLiteral("Disconnected"));
    }

    void parseStatusFields_keysAreLowercased()
    {
        // Keys must be stored in lowercase regardless of how the CLI capitalises them.
        const QString input = QStringLiteral("STATUS:  Connected\nSERVER:  DE#42\n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QVERIFY(fields.contains(QStringLiteral("status")));
        QVERIFY(fields.contains(QStringLiteral("server")));
        // The original-case keys must NOT be present.
        QVERIFY(fields.contains(QStringLiteral("STATUS")) == false);
    }

    void parseStatusFields_valuesPreserveCase()
    {
        // Values are NOT normalised - they are kept exactly as output by the CLI.
        const QString input = QStringLiteral("Status:  Connected\n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QCOMPARE(fields.value(QStringLiteral("status")), QStringLiteral("Connected"));
    }

    void parseStatusFields_leadingTrailingWhitespace_isTrimmed()
    {
        const QString input = QStringLiteral("  Status  :   Connected   \n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QCOMPARE(fields.value(QStringLiteral("status")), QStringLiteral("Connected"));
    }

    void parseStatusFields_linesWithoutColon_areSkipped()
    {
        const QString input =
            QStringLiteral("ProtonVPN CLI v3.0\n"
                           "Status:  Connected\n"
                           "Some random line without a colon\n");

        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QCOMPARE(fields.size(), 1);
        QVERIFY(fields.contains(QStringLiteral("status")));
    }

    void parseStatusFields_emptyInput_returnsEmptyMap()
    {
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(QString());
        QVERIFY(fields.isEmpty());
    }

    void parseStatusFields_traceback_hasNoStatusField()
    {
        // A crashed CLI must NOT look like a disconnect: callers key off the
        // presence of "status" to decide whether the snapshot is usable.
        const QString input =
            QStringLiteral("Traceback (most recent call last):\n"
                           "  File \"/usr/bin/protonvpn\", line 8, in <module>\n"
                           "RuntimeError: daemon unavailable\n");

        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);

        QVERIFY(fields.contains(QStringLiteral("status")) == false);
    }

    void parseStatusFields_noActiveConnectionProse_synthesizesDisconnected()
    {
        // Some CLI versions print a sentence instead of a "Status:" line.
        const QString input = QStringLiteral("There is no active Proton VPN connection.\n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QCOMPARE(fields.value(QStringLiteral("status")), QStringLiteral("Disconnected"));
    }

    void parseStatusFields_notConnectedProse_synthesizesDisconnected()
    {
        const QString input = QStringLiteral("You are not connected.\n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QCOMPARE(fields.value(QStringLiteral("status")), QStringLiteral("Disconnected"));
    }

    void parseStatusFields_explicitStatus_isNotOverwrittenBySynthesis()
    {
        const QString input = QStringLiteral("Status: Connected\nServer: DE#42\n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QCOMPARE(fields.value(QStringLiteral("status")), QStringLiteral("Connected"));
    }

    void parseStatusFields_noiseLines_areRemoved()
    {
        // Lines containing noise keywords must be stripped before parsing.
        const QString input =
            QStringLiteral("Status:  Connected\n"
                           "This version is outdated, please update.\n"
                           "Updating server list, this may take a moment...\n"
                           "Guide: https://protonvpn.com/support/port-forwarding\n"
                           "To get your forwarded port, run natpmpc.\n"
                           "natpmpc -a 1 0 udp 60\n"
                           "Server:  DE#42\n");

        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);

        // Only "status" and "server" should survive - all noise must be gone.
        QCOMPARE(fields.size(), 2);
        QVERIFY(fields.contains(QStringLiteral("status")));
        QVERIFY(fields.contains(QStringLiteral("server")));
    }

    void parseStatusFields_noiseKeyword_outdated_filtered()
    {
        const QString input =
            QStringLiteral("Note: This is outdated.\nStatus: Connected\n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        // "Note" line contained "outdated" -> must be removed; only status survives.
        QCOMPARE(fields.size(), 1);
    }

    void parseStatusFields_multipleValuesWithColonsInValue_parsedCorrectly()
    {
        // A value may itself contain a colon (e.g. an IPv6 address or timestamp).
        // Only the FIRST colon is the separator - the rest belongs to the value.
        const QString input =
            QStringLiteral("IP:  2001:db8::1\n");
        const QMap<QString, QString> fields = StatusMonitor::parseStatusFields(input);
        QCOMPARE(fields.value(QStringLiteral("ip")), QStringLiteral("2001:db8::1"));
    }

    //  parseCityFromServer

    void parseCityFromServer_fullServerString_returnsCity()
    {
        const QString city =
            StatusMonitor::parseCityFromServer(
                QStringLiteral("US-NJ#203 in Secaucus, United States"));
        QCOMPARE(city, QStringLiteral("Secaucus"));
    }

    void parseCityFromServer_differentCity_returnsCorrectCity()
    {
        const QString city =
            StatusMonitor::parseCityFromServer(
                QStringLiteral("DE#42 in Frankfurt, Germany"));
        QCOMPARE(city, QStringLiteral("Frankfurt"));
    }

    void parseCityFromServer_noInKeyword_returnsEmpty()
    {
        const QString city = StatusMonitor::parseCityFromServer(QStringLiteral("DE#42"));
        QVERIFY(city.isEmpty());
    }

    void parseCityFromServer_emptyString_returnsEmpty()
    {
        QVERIFY(StatusMonitor::parseCityFromServer(QString()).isEmpty());
    }

    void parseCityFromServer_cityWithSpaces_returnsFullCity()
    {
        // "New York" has a space inside the city name - must be returned in full.
        const QString city =
            StatusMonitor::parseCityFromServer(
                QStringLiteral("US-NY#10 in New York, United States"));
        QCOMPARE(city, QStringLiteral("New York"));
    }

    void parseCityFromServer_whitespaceAroundCity_isTrimmed()
    {
        const QString city =
            StatusMonitor::parseCityFromServer(
                QStringLiteral("CH#7 in  Zurich , Switzerland"));
        QCOMPARE(city, QStringLiteral("Zurich"));
    }

    void parseCityFromServer_serverStringWithNoCountry_returnsCity()
    {
        // No comma -> entire remainder after " in " is the city.
        const QString city =
            StatusMonitor::parseCityFromServer(QStringLiteral("JP#1 in Tokyo"));
        QCOMPARE(city, QStringLiteral("Tokyo"));
    }

    //  parseServerName

    void parseServerName_fullServerString_returnsName()
    {
        // The name is what `protonvpn connect <name>` takes to reconnect.
        QCOMPARE(StatusMonitor::parseServerName(
                     QStringLiteral("US-NJ#203 in Secaucus, United States")),
                 QStringLiteral("US-NJ#203"));
    }

    void parseServerName_nameOnly_returnsName()
    {
        QCOMPARE(StatusMonitor::parseServerName(QStringLiteral("DE#42")), QStringLiteral("DE#42"));
        QVERIFY(StatusMonitor::parseServerName(QString()).isEmpty());
    }

    //  parseCountryFromServer

    void parseCountryFromServer_regionServer_returnsCountry()
    {
        QCOMPARE(StatusMonitor::parseCountryFromServer(
                     QStringLiteral("US-NJ#203 in Secaucus, United States")),
                 QStringLiteral("US"));
    }

    void parseCountryFromServer_plainServer_returnsCountry()
    {
        QCOMPARE(StatusMonitor::parseCountryFromServer(QStringLiteral("de#42 in Berlin, Germany")),
                 QStringLiteral("DE"));
    }

    void parseCountryFromServer_dashInLocation_isIgnored()
    {
        // Only the name counts; a dash in the location must not be read as one.
        QCOMPARE(StatusMonitor::parseCountryFromServer(
                     QStringLiteral("FR#5 in Saint-Denis, France")),
                 QStringLiteral("FR"));
    }

    void parseCountryFromServer_secureCoreServer_returnsExitCountry()
    {
        // "<entry>-<exit>#n": the connection comes out in the second country.
        QCOMPARE(StatusMonitor::parseCountryFromServer(
                     QStringLiteral("CH-US#1 in New York, United States")),
                 QStringLiteral("US"));
        QCOMPARE(StatusMonitor::parseCountryFromServer(QStringLiteral("SE-JP#1")), QStringLiteral("JP"));
        QCOMPARE(StatusMonitor::parseCountryFromServer(QStringLiteral("is-de#2")), QStringLiteral("DE"));
    }

    void parseCountryFromServer_stateThatLooksLikeACountry_returnsUs()
    {
        // California, Delaware and Colorado, not Canada, Germany and Colombia.
        QCOMPARE(StatusMonitor::parseCountryFromServer(QStringLiteral("US-CA#77")), QStringLiteral("US"));
        QCOMPARE(StatusMonitor::parseCountryFromServer(QStringLiteral("US-DE#3")), QStringLiteral("US"));
        QCOMPARE(StatusMonitor::parseCountryFromServer(QStringLiteral("US-CO#21-TOR")), QStringLiteral("US"));
    }

    void parseCountryFromServer_tagAfterDash_returnsCountry()
    {
        QCOMPARE(StatusMonitor::parseCountryFromServer(QStringLiteral("JP-FREE#1")), QStringLiteral("JP"));
        // A Secure Core entry country's own tagged and plain servers.
        QCOMPARE(StatusMonitor::parseCountryFromServer(QStringLiteral("CH-FREE#4")), QStringLiteral("CH"));
        QCOMPARE(StatusMonitor::parseCountryFromServer(QStringLiteral("CH#18-TOR")), QStringLiteral("CH"));
        QCOMPARE(StatusMonitor::parseCountryFromServer(QStringLiteral("SE#31")), QStringLiteral("SE"));
    }

    void parseCountryFromServer_noPrefix_returnsEmpty()
    {
        QVERIFY(StatusMonitor::parseCountryFromServer(QStringLiteral("server in Nowhere")).isEmpty());
        QVERIFY(StatusMonitor::parseCountryFromServer(QString()).isEmpty());
    }

    //  loopCommand

    void loopCommand_interval_sleepsThatLongBetweenRuns()
    {
        const QString command = StatusMonitor::loopCommand(QStringLiteral("protonvpn status"), 30);

        QVERIFY(command.contains(QStringLiteral("do protonvpn status 2>&1;")));
        QVERIFY(command.contains(QStringLiteral("sleep 30;")));
    }

    void loopCommand_noPeriodicPoll_runsOnceAndWaits()
    {
        // One snapshot per start or restart, then nothing until the next one.
        const QString command = StatusMonitor::loopCommand(QStringLiteral("protonvpn status"),
                                                           StatusMonitor::NO_PERIODIC_POLL);

        QVERIFY(command.contains(QStringLiteral("do protonvpn status 2>&1;")));
        QVERIFY(command.contains(QStringLiteral("sleep infinity;")));
    }

    void loopCommand_flatpakStatusCommand_isUsedAsGiven()
    {
        const QString command = StatusMonitor::loopCommand(
            QStringLiteral("flatpak-spawn --host protonvpn status"), 15);

        QVERIFY(command.contains(QStringLiteral("do flatpak-spawn --host protonvpn status 2>&1;")));
        QVERIFY(command.contains(QStringLiteral("sleep 15;")));
    }
};

QTEST_MAIN(TstStatusMonitor)
#include "tst_statusMonitor.moc"

