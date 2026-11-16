#pragma once

// cliDisconnect.h
// Reads the result of `protonvpn disconnect`. Lives in its own header so
// every caller judges it the same way, and so that can be unit tested.

#include <QLatin1Char>
#include <QLatin1String>
#include <QString>
#include <QStringList>
#include <algorithm>

namespace CliDisconnect
{
// True when the output contains the CLI's own "Disconnected." confirmation.
//
// `protonvpn disconnect` prints it only once the tunnel is down, but CLI 1.0.3
// can still exit non-zero afterwards: it then waits on a background location
// refresh that api-core starts on disconnect, and that has been seen to end
// the process with exit code 1 and no output at all. The line is therefore a
// more reliable answer to "did the disconnect happen?" than the exit code.
inline bool confirmed(const QString& output)
{
    const QStringList lines = output.split(QLatin1Char('\n'));
    return std::ranges::any_of(lines, [](const QString& line)
    {
        return line.trimmed() == QLatin1String("Disconnected.");
    });
}

// True when the disconnect happened: the CLI exited cleanly or confirmed it.
inline bool succeeded(const int exitCode, const QString& output)
{
    return exitCode == 0 || confirmed(output) == true;
}
} // namespace CliDisconnect
