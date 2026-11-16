#include <QtTest/QtTest>
#include "cli/cliInstallCheck.h"

// CliInstallCheck::ranCli() decides between the "not installed" page and the
// login check. In the Flatpak a missing CLI still produces output (from
// flatpak-spawn), which used to count as installed.

class TstCliInstallCheck : public QObject
{
    Q_OBJECT

private slots:
    void ranCli_helpOutput_isInstalled()
    {
        QVERIFY(CliInstallCheck::ranCli(QStringLiteral("Usage: protonvpn [OPTIONS] COMMAND [ARGS]...\n"), QString()));
    }

    void ranCli_outputOnStandardErrorOnly_isInstalled()
    {
        // A CLI that starts but fails (a traceback, say) is installed; the
        // login check reports what is wrong with it.
        QVERIFY(CliInstallCheck::ranCli(QString(), QStringLiteral("Traceback (most recent call last):\n")));
    }

    void ranCli_noOutput_isNotInstalled()
    {
        QCOMPARE(CliInstallCheck::ranCli(QString(), QString()), false);
        QCOMPARE(CliInstallCheck::ranCli(QStringLiteral("  \n"), QStringLiteral("\n")), false);
    }

    void ranCli_flatpakSpawnMissingCommand_isNotInstalled()
    {
        // flatpak-spawn's exact message for a host command that does not exist.
        const QString message = QStringLiteral(
            "Portal call failed: Failed to start command: Failed to execute child process "
            "“protonvpn” (No such file or directory)\n");

        QCOMPARE(CliInstallCheck::ranCli(QString(), message), false);
        QCOMPARE(CliInstallCheck::ranCli(message, QString()), false);
    }
};

QTEST_MAIN(TstCliInstallCheck)
#include "tst_cliInstallCheck.moc"
