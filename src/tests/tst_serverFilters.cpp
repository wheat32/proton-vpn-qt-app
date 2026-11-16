#include <QtTest/QtTest>
#include "cli/serverFilters.h"

// The Countries page's filter rules. The P2P filter used to be cleared by
// every settings read, and the accordion once listed cities its filters
// should have hidden; both now go through these functions.

class TstServerFilters : public QObject
{
    Q_OBJECT

private slots:
    //  hasFeature

    void hasFeature_listedFeature_isTrue()
    {
        QVERIFY(ServerFilters::hasFeature(QStringLiteral("P2P, Tor"), "p2p"));
        QVERIFY(ServerFilters::hasFeature(QStringLiteral("P2P, Tor"), "tor"));
        QVERIFY(ServerFilters::hasFeature(QStringLiteral("P2P, Secure Core"), "secure core"));
    }

    void hasFeature_missingFeatureOrNone_isFalse()
    {
        QCOMPARE(ServerFilters::hasFeature(QStringLiteral("P2P"), "tor"), false);
        QCOMPARE(ServerFilters::hasFeature(QString(), "p2p"), false);
    }

    //  cityPassesFilters

    void cityPassesFilters_noFilters_listsEveryCity()
    {
        QVERIFY(ServerFilters::cityPassesFilters(QString(), false, false, false));
        QVERIFY(ServerFilters::cityPassesFilters(QStringLiteral("P2P"), false, false, false));
    }

    void cityPassesFilters_oneFilter_needsThatFeature()
    {
        QVERIFY(ServerFilters::cityPassesFilters(QStringLiteral("P2P"), true, false, false));
        QCOMPARE(ServerFilters::cityPassesFilters(QString(), true, false, false), false);
        QCOMPARE(ServerFilters::cityPassesFilters(QStringLiteral("Tor"), true, false, false), false);
    }

    void cityPassesFilters_severalFilters_needAllOfThem()
    {
        QVERIFY(ServerFilters::cityPassesFilters(QStringLiteral("P2P, Tor"), true, false, true));
        QCOMPARE(ServerFilters::cityPassesFilters(QStringLiteral("P2P"), true, false, true), false);
        QCOMPARE(ServerFilters::cityPassesFilters(QStringLiteral("P2P, Tor"), true, true, true), false);
    }

    //  p2pFilterAfterSettingsRead

    void p2pFilterAfterSettingsRead_forwardingUnchanged_keepsTheUsersFilter()
    {
        // Port forwarding off before and after: a filter set by hand stays.
        QCOMPARE(ServerFilters::p2pFilterAfterSettingsRead(false, false, true), true);
        QCOMPARE(ServerFilters::p2pFilterAfterSettingsRead(false, false, false), false);
        // On before and after: a filter cleared by hand stays cleared.
        QCOMPARE(ServerFilters::p2pFilterAfterSettingsRead(true, true, false), false);
        QCOMPARE(ServerFilters::p2pFilterAfterSettingsRead(true, true, true), true);
    }

    void p2pFilterAfterSettingsRead_forwardingTurnedOn_turnsFilterOn()
    {
        QCOMPARE(ServerFilters::p2pFilterAfterSettingsRead(false, true, false), true);
    }

    void p2pFilterAfterSettingsRead_forwardingTurnedOff_turnsFilterOff()
    {
        QCOMPARE(ServerFilters::p2pFilterAfterSettingsRead(true, false, true), false);
    }
};

QTEST_MAIN(TstServerFilters)
#include "tst_serverFilters.moc"
