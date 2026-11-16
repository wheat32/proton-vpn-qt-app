#include <QtTest/QtTest>
#include "appVersion.h"

// AppVersion::parse() reads version.json, which every version shown or
// compared in the app comes from.

class TstAppVersion : public QObject
{
    Q_OBJECT

private slots:
    void parse_fullFile_readsEveryField()
    {
        const AppVersion::Info info = AppVersion::parse(
            R"({"app_version": "1.13.1", "prerelease": true,
                "cli_version_tested_min": "1.0.0", "cli_version_tested_max": "1.1.0"})");

        QCOMPARE(info.appVersion, QStringLiteral("1.13.1"));
        QCOMPARE(info.cliTestedMin, QStringLiteral("1.0.0"));
        QCOMPARE(info.cliTestedMax, QStringLiteral("1.1.0"));
        QCOMPARE(info.prerelease, true);
    }

    void parse_missingKeys_leavesFieldsEmpty()
    {
        const AppVersion::Info info = AppVersion::parse(R"({"app_version": "2.0.0"})");

        QCOMPARE(info.appVersion, QStringLiteral("2.0.0"));
        QVERIFY(info.cliTestedMin.isEmpty());
        QVERIFY(info.cliTestedMax.isEmpty());
        QCOMPARE(info.prerelease, false);
    }

    void parse_invalidJson_givesEmptyInfo()
    {
        const AppVersion::Info info = AppVersion::parse("not json");

        QVERIFY(info.appVersion.isEmpty());
        QCOMPARE(info.prerelease, false);
    }

    void embedded_matchesTheProjectsVersionFile()
    {
        // The test embeds the real version.json, so a key renamed there
        // without updating the reader fails here.
        const AppVersion::Info& info = AppVersion::embedded();

        QVERIFY(info.appVersion.isEmpty() == false);
        QVERIFY(info.cliTestedMin.isEmpty() == false);
        QVERIFY(info.cliTestedMax.isEmpty() == false);
    }
};

QTEST_MAIN(TstAppVersion)
#include "tst_appVersion.moc"
