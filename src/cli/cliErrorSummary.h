#pragma once

// cliErrorSummary.h
// Shortens an error the CLI reported to what fits on the VPN page. Lives in
// its own header so the limits can be unit tested.

#include <QLatin1Char>
#include <QString>
#include <QStringList>

namespace CliErrorSummary
{
// The most of a CLI error message the page shows itself; the rest is behind
// "View Details".
constexpr int MAX_LINES = 4;
constexpr int MAX_CHARS = 300;

// What the page shows of an error the CLI reported: its own words, down to the
// first few lines and a bounded length so a long message cannot take over the
// page. Empty when the CLI said nothing. A shortened message ends in an
// ellipsis.
inline QString summarize(const QString& cliOutput)
{
    QStringList lines;
    bool shortened = false;
    const QStringList rawLines = cliOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& rawLine : rawLines)
    {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() == true) continue;
        if (lines.size() == MAX_LINES)
        {
            shortened = true;
            break;
        }
        lines.append(line);
    }

    QString summary = lines.join(QLatin1Char('\n'));
    if (summary.size() > MAX_CHARS)
    {
        summary = summary.left(MAX_CHARS).trimmed();
        shortened = true;
    }
    if (shortened == true)
    {
        summary += QStringLiteral("…");
    }
    return summary;
}
} // namespace CliErrorSummary
