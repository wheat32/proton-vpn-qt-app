#include <QtTest/QtTest>
#include "cli/cliErrorSummary.h"

// CliErrorSummary::summarize() bounds what the VPN page shows of a CLI error,
// so that a long message cannot push the page's controls out of view.

class TstCliErrorSummary : public QObject
{
    Q_OBJECT

private slots:
    void summarize_shortMessage_isShownWhole()
    {
        QCOMPARE(CliErrorSummary::summarize(QStringLiteral("Error: Invalid country code 'GB'.")),
                 QStringLiteral("Error: Invalid country code 'GB'."));
    }

    void summarize_emptyOrBlankOutput_isEmpty()
    {
        QVERIFY(CliErrorSummary::summarize(QString()).isEmpty());
        QVERIFY(CliErrorSummary::summarize(QStringLiteral("\n  \n\n")).isEmpty());
    }

    void summarize_blankLinesAndPadding_areDropped()
    {
        QCOMPARE(CliErrorSummary::summarize(QStringLiteral("  first  \n\n   \nsecond\n")),
                 QStringLiteral("first\nsecond"));
    }

    void summarize_tooManyLines_keepsTheFirstAndAddsEllipsis()
    {
        QStringList lines;
        for (int i = 1; i <= CliErrorSummary::MAX_LINES + 3; ++i)
        {
            lines << QStringLiteral("line %1").arg(i);
        }

        const QString summary = CliErrorSummary::summarize(lines.join(QLatin1Char('\n')));

        QCOMPARE(summary.count(QLatin1Char('\n')), CliErrorSummary::MAX_LINES - 1);
        QVERIFY(summary.startsWith(QStringLiteral("line 1\n")));
        QVERIFY(summary.endsWith(QStringLiteral("…")));
    }

    void summarize_exactlyTheLineLimit_hasNoEllipsis()
    {
        QStringList lines;
        for (int i = 1; i <= CliErrorSummary::MAX_LINES; ++i)
        {
            lines << QStringLiteral("line %1").arg(i);
        }

        QCOMPARE(CliErrorSummary::summarize(lines.join(QLatin1Char('\n'))), lines.join(QLatin1Char('\n')));
    }

    void summarize_oneVeryLongLine_isCutToTheCharacterLimit()
    {
        const QString summary = CliErrorSummary::summarize(QString(CliErrorSummary::MAX_CHARS * 2, QLatin1Char('x')));

        QCOMPARE(summary.size(), CliErrorSummary::MAX_CHARS + 1); // the limit, plus the ellipsis
        QVERIFY(summary.endsWith(QStringLiteral("…")));
    }
};

QTEST_MAIN(TstCliErrorSummary)
#include "tst_cliErrorSummary.moc"
