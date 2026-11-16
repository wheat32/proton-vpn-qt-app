#include <QtTest/QtTest>
#include "cli/cliDisconnect.h"

// CliDisconnect reads the result of `protonvpn disconnect`. CLI 1.0.3 can
// confirm the disconnect and still exit non-zero, and every caller has to
// take that as a success: the "apply and reconnect" path once did not, and
// stopped with an empty error and the VPN left down.

class TstCliDisconnect : public QObject
{
    Q_OBJECT

private slots:
    void confirmed_disconnectedLine_isTrue()
    {
        QVERIFY(CliDisconnect::confirmed(QStringLiteral("Disconnected.")));
        QVERIFY(CliDisconnect::confirmed(QStringLiteral("Disconnecting...\n  Disconnected.  \n")));
    }

    void confirmed_otherOutput_isFalse()
    {
        QCOMPARE(CliDisconnect::confirmed(QString()), false);
        QCOMPARE(CliDisconnect::confirmed(QStringLiteral("Error: not connected")), false);
        // Only the whole line counts, not text that merely contains the word.
        QCOMPARE(CliDisconnect::confirmed(QStringLiteral("Status: Disconnected. Try again")), false);
    }

    void succeeded_cleanExit_isTrue()
    {
        QVERIFY(CliDisconnect::succeeded(0, QString()));
    }

    void succeeded_confirmedButNonZeroExit_isTrue()
    {
        QVERIFY(CliDisconnect::succeeded(1, QStringLiteral("Disconnected.")));
    }

    void succeeded_failedWithoutConfirmation_isFalse()
    {
        QCOMPARE(CliDisconnect::succeeded(1, QStringLiteral("Error: something went wrong")), false);
        QCOMPARE(CliDisconnect::succeeded(1, QString()), false);
    }
};

QTEST_MAIN(TstCliDisconnect)
#include "tst_cliDisconnect.moc"
