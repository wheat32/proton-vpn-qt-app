#include <QtTest/QtTest>
#include "cli/connectQueue.h"

// ConnectQueue keeps connect requests in order. Two bugs came from not having
// it: a startup auto-connect that connected to its own destination after the
// user had picked another, and two `protonvpn connect` commands running side
// by side.

class TstConnectQueue : public QObject
{
    Q_OBJECT

private slots:
    void supersede_makesOlderTicketsStale()
    {
        ConnectQueue queue;
        const int autoConnect = queue.supersede();
        QVERIFY(queue.isCurrent(autoConnect));

        // The user connects somewhere else while the auto-connect is waiting.
        const int manual = queue.supersede();

        QCOMPARE(queue.isCurrent(autoConnect), false);
        QVERIFY(queue.isCurrent(manual));
    }

    void commandRunning_followsStartAndFinish()
    {
        ConnectQueue queue;
        QCOMPARE(queue.commandRunning(), false);
        queue.commandStarted();
        QVERIFY(queue.commandRunning());
        queue.commandFinished();
        QCOMPARE(queue.commandRunning(), false);
    }

    void takeHeld_withoutRequest_isEmpty()
    {
        ConnectQueue queue;
        QCOMPARE(queue.hasHeld(), false);
        QCOMPARE(queue.takeHeld().has_value(), false);
    }

    void hold_thenTake_returnsRequestOnce()
    {
        ConnectQueue queue;
        queue.hold({.country = QStringLiteral("CH"), .city = QStringLiteral("Zurich"), .server = QString()});
        QVERIFY(queue.hasHeld());

        const std::optional<ConnectRequest> held = queue.takeHeld();
        QVERIFY(held.has_value());
        QCOMPARE(held->country, QStringLiteral("CH"));
        QCOMPARE(held->city, QStringLiteral("Zurich"));

        QCOMPARE(queue.hasHeld(), false);
        QCOMPARE(queue.takeHeld().has_value(), false);
    }

    void hold_twice_keepsNewestRequest()
    {
        ConnectQueue queue;
        queue.hold({.country = QStringLiteral("US"), .city = QString(), .server = QString()});
        queue.hold({.country = QStringLiteral("CH"), .city = QString(), .server = QString()});

        QCOMPARE(queue.takeHeld()->country, QStringLiteral("CH"));
    }

    void dropHeld_discardsRequest()
    {
        ConnectQueue queue;
        queue.hold({.country = QStringLiteral("US"), .city = QString(), .server = QString()});
        queue.dropHeld();

        QCOMPARE(queue.hasHeld(), false);
    }
};

QTEST_MAIN(TstConnectQueue)
#include "tst_connectQueue.moc"
