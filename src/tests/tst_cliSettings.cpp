#include <QtTest/QtTest>
#include "cli/cliSettings.h"

// The VPN tab shows what `protonvpn config list` reports, so a parsing slip
// shows the user a setting that is not what the CLI uses. The samples are
// real CLI 1.0.3 output; the free-plan and custom DNS rows follow the CLI's
// formatting code (feature_setting_definitions.py and settings.py).

namespace
{
const QString PLUS_DEFAULTS = QStringLiteral(
    "\n"
    "Current configuration\n"
    "Setting                  Value\n"
    "-----------------------  ------------\n"
    "netshield                malware-only\n"
    "kill-switch              off\n"
    "port-forwarding          off\n"
    "custom-dns               off\n"
    "vpn-accelerator          on\n"
    "moderate-nat             off\n"
    "ipv6                     on\n"
    "anonymous-crash-reports  on\n"
    "\n"
    "Use 'protonvpn config set <setting> <value>' to change settings.\n"
    "Use 'protonvpn config set <setting> --help' for available values.\n");
} // namespace

class TstCliSettings : public QObject
{
    Q_OBJECT

private slots:
    // parseConfigList ---------------------------------------------------------

    void parseConfigList_plusDefaults_readsEverySetting()
    {
        const QMap<QString, QString> s = CliSettings::parseConfigList(PLUS_DEFAULTS);
        QCOMPARE(s.size(), 8);
        QCOMPARE(s.value(QStringLiteral("netshield")), QStringLiteral("malware-only"));
        QCOMPARE(s.value(QStringLiteral("kill-switch")), QStringLiteral("off"));
        QCOMPARE(s.value(QStringLiteral("vpn-accelerator")), QStringLiteral("on"));
        QCOMPARE(s.value(QStringLiteral("ipv6")), QStringLiteral("on"));
        QCOMPARE(s.value(QStringLiteral("anonymous-crash-reports")), QStringLiteral("on"));
    }

    void parseConfigList_footerAfterTable_isNotASetting()
    {
        const QMap<QString, QString> s = CliSettings::parseConfigList(PLUS_DEFAULTS);
        QVERIFY(s.contains(QStringLiteral("Use")) == false);
    }

    void parseConfigList_freePlan_keepsUpgradeText()
    {
        const QMap<QString, QString> s = CliSettings::parseConfigList(QStringLiteral(
            "Current configuration\n"
            "Setting                  Value\n"
            "-----------------------  -----------------\n"
            "netshield                Upgrade to enable\n"
            "kill-switch              standard\n"
            "\n"
            "To upgrade to VPN Plus visit: https://account.protonvpn.com/pricing\n"));
        QCOMPARE(s.value(QStringLiteral("netshield")), QStringLiteral("Upgrade to enable"));
        QCOMPARE(s.value(QStringLiteral("kill-switch")), QStringLiteral("standard"));
    }

    void parseConfigList_customDnsWithServers_keepsWholeValue()
    {
        const QMap<QString, QString> s = CliSettings::parseConfigList(QStringLiteral(
            "Setting     Value\n"
            "----------  ---------------------------\n"
            "custom-dns  on  [1.1.1.1, 8.8.8.8, ...]\n"));
        QCOMPARE(s.value(QStringLiteral("custom-dns")), QStringLiteral("on  [1.1.1.1, 8.8.8.8, ...]"));
    }

    void parseConfigList_error_isEmpty()
    {
        QVERIFY(CliSettings::parseConfigList(QStringLiteral(
            "Error: Authentication required to view settings. Please sign in with 'protonvpn signin'"))
            .isEmpty());
        QVERIFY(CliSettings::parseConfigList(QString()).isEmpty());
    }

    // customDnsServers --------------------------------------------------------

    void customDnsServers_twoServers_isComplete()
    {
        bool truncated = true;
        QCOMPARE(CliSettings::customDnsServers(QStringLiteral("on  [1.1.1.1, 8.8.8.8]"), &truncated),
                 QStringList({QStringLiteral("1.1.1.1"), QStringLiteral("8.8.8.8")}));
        QCOMPARE(truncated, false);
    }

    void customDnsServers_moreThanShown_isTruncated()
    {
        bool truncated = false;
        QCOMPARE(CliSettings::customDnsServers(QStringLiteral("on  [1.1.1.1, 2606:4700:4700::1111, ...]"),
                                               &truncated),
                 QStringList({QStringLiteral("1.1.1.1"), QStringLiteral("2606:4700:4700::1111")}));
        QCOMPARE(truncated, true);
    }

    void customDnsServers_noList_isEmpty()
    {
        QVERIFY(CliSettings::customDnsServers(QStringLiteral("on")).isEmpty());
        QVERIFY(CliSettings::customDnsServers(QStringLiteral("off")).isEmpty());
    }

    // customDnsValue ----------------------------------------------------------

    void customDnsValue_servers_areCommaSeparated()
    {
        QCOMPARE(CliSettings::customDnsValue({QStringLiteral("1.1.1.1"), QStringLiteral("8.8.8.8")}),
                 QStringLiteral("1.1.1.1,8.8.8.8"));
    }

    void customDnsValue_noServers_isOn()
    {
        QCOMPARE(CliSettings::customDnsValue({}), QStringLiteral("on"));
    }

    // normalizeDnsList --------------------------------------------------------

    void normalizeDnsList_spacesAfterCommas_areRemoved()
    {
        QCOMPARE(CliSettings::normalizeDnsList(QStringLiteral(" 1.1.1.1, 8.8.8.8 ")),
                 QStringLiteral("1.1.1.1,8.8.8.8"));
    }

    void normalizeDnsList_spacesOnlyOrDoubledCommas_areSeparators()
    {
        QCOMPARE(CliSettings::normalizeDnsList(QStringLiteral("1.1.1.1 8.8.8.8,,9.9.9.9")),
                 QStringLiteral("1.1.1.1,8.8.8.8,9.9.9.9"));
    }

    void normalizeDnsList_blank_isEmpty()
    {
        QVERIFY(CliSettings::normalizeDnsList(QStringLiteral(" , ")).isEmpty());
    }

    //  offersPlusUpgrade

    void offersPlusUpgrade_freeAccountOutput_isTrue()
    {
        const QString output = QStringLiteral(
            "Setting                  Value\n"
            "-----------------------  ------------\n"
            "netshield                off\n"
            "\n"
            "To upgrade to VPN Plus, visit https://account.protonvpn.com\n");
        QVERIFY(CliSettings::offersPlusUpgrade(output));
    }

    void offersPlusUpgrade_plusAccountOrNoOutput_isFalse()
    {
        const QString output = QStringLiteral(
            "Setting                  Value\n"
            "-----------------------  ------------\n"
            "netshield                malware-only\n");
        QCOMPARE(CliSettings::offersPlusUpgrade(output), false);
        QCOMPARE(CliSettings::offersPlusUpgrade(QString()), false);
    }
};

QTEST_MAIN(TstCliSettings)
#include "tst_cliSettings.moc"
