#pragma once

// cliInstallCheck.h
// Tells whether running `protonvpn --help` reached the CLI. Lives in its own
// header so the rule can be unit tested.

#include <QString>

namespace CliInstallCheck
{
namespace Detail
{
// How flatpak-spawn starts its message when the host command could not be
// run, for example:
//   Portal call failed: Failed to start command: Failed to execute child
//   process "protonvpn" (No such file or directory)
inline const QString SPAWN_FAILURE_MARK = QStringLiteral("Portal call failed");
} // namespace Detail

// True when the output of `protonvpn --help` came from the CLI itself.
//
// A program that is not there prints nothing natively (QProcess fails to start
// it). In the Flatpak the command is `flatpak-spawn --host protonvpn`, which
// always starts and reports a missing host command with a message of its own,
// so "any output" is not enough to call the CLI installed.
inline bool ranCli(const QString& standardOutput, const QString& standardError)
{
    const QString combined = standardOutput + standardError;
    if (combined.trimmed().isEmpty() == true) return false;
    return combined.contains(Detail::SPAWN_FAILURE_MARK) == false;
}
} // namespace CliInstallCheck
