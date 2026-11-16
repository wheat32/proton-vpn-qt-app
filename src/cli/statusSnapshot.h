#pragma once

// statusSnapshot.h
// Decides what one `protonvpn status` snapshot changes: the connection state,
// the connected server, and what has to be announced. Lives in its own header,
// free of VpnManager, so the rules can be unit tested.

#include <QLatin1String>
#include <QMap>
#include <QString>
#include "../vpnState.h"

namespace StatusSnapshot
{
// The state and server after a snapshot, and what about them is news.
struct Outcome
{
    VpnState state = VpnState::Unknown; // the state afterwards
    QString  server;                    // the connected server afterwards; empty unless connected
    bool readable      = false; // the snapshot had a status; without one nothing changes
    bool stateChanged  = false; // `state` is new and has to be announced
    bool serverChanged = false; // still connected, but to another server (announced like a new connection)
    bool serverLearned = false; // the server of a connection that was already up is now known
};

// Applies the fields of a snapshot (StatusMonitor::parseStatusFields()) to the
// current state and connected server.
//
// - A snapshot without a "status" field could not be read (empty output, a
//   traceback, a hung CLI). It says nothing, and above all not "disconnected".
// - Error stays until something better is known. A failed connect or
//   disconnect leaves Error on screen with what went wrong; "disconnected" is
//   no news then, and announcing it would wipe the message a few seconds
//   after it appeared.
// - A connection the app made itself starts without a server (the CLI's
//   connect output is not parsed for one); the first snapshot fills it in.
//
// The caller skips snapshots that arrive while a connect or disconnect is in
// progress.
inline Outcome apply(const VpnState current, const QString& currentServer,
                     const QMap<QString, QString>& fields)
{
    Outcome outcome;
    outcome.state  = current;
    outcome.server = currentServer;

    const QString statusKey = QStringLiteral("status");
    if (fields.contains(statusKey) == false)
    {
        return outcome;
    }
    outcome.readable = true;

    const bool connected = fields.value(statusKey).toLower() == QLatin1String("connected");
    const VpnState reported = connected == true ? VpnState::Connected : VpnState::Disconnected;
    const QString server = connected == true ? fields.value(QStringLiteral("server")) : QString();

    if (current == VpnState::Error && reported == VpnState::Disconnected)
    {
        outcome.server.clear();
        return outcome;
    }

    if (reported != current)
    {
        outcome.state        = reported;
        outcome.server       = server;
        outcome.stateChanged = true;
        return outcome;
    }

    if (connected == false)
    {
        outcome.server.clear();
        return outcome;
    }

    if (server.isEmpty() == true || server == currentServer)
    {
        return outcome;
    }

    outcome.server = server;
    if (currentServer.isEmpty() == true)
    {
        outcome.serverLearned = true;
    }
    else
    {
        outcome.serverChanged = true;
    }
    return outcome;
}
} // namespace StatusSnapshot
