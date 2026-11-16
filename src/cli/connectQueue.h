#pragma once

// connectQueue.h
// Keeps connect requests in order: one CLI connect or disconnect runs at a
// time, the newest request wins, and anything still waiting from an older
// request (a startup auto-connect's network wait or retry) is called off.
// VpnManager owns one; it holds no process itself, so the rules can be unit
// tested.
//
// Without it a second request started a second `protonvpn connect` beside the
// first, and a startup auto-connect that was still waiting connected to its
// own destination after the user had already chosen another.

#include <QString>
#include <optional>

// Where a connection goes. A server name, when given, wins over country/city.
struct ConnectRequest
{
    QString country;
    QString city;
    QString server;
};

class ConnectQueue
{
public:
    // A new connect or disconnect was asked for: everything asked for earlier
    // is out of date from here on. Returns the ticket of the new request.
    int supersede()
    {
        ++m_ticket;
        return m_ticket;
    }

    // Whether `ticket` still belongs to the newest request. Deferred steps
    // (a timer, a finished command) check this before acting.
    [[nodiscard]] bool isCurrent(const int ticket) const { return ticket == m_ticket; }

    // A CLI connect or disconnect is running; no second one may start.
    void commandStarted() { m_commandRunning = true; }
    void commandFinished() { m_commandRunning = false; }
    [[nodiscard]] bool commandRunning() const { return m_commandRunning; }

    // Holds `request` until the running command finishes, replacing any
    // request already held.
    void hold(const ConnectRequest& request) { m_held = request; }
    [[nodiscard]] bool hasHeld() const { return m_held.has_value(); }

    // The held request, which is then no longer held; nullopt without one.
    [[nodiscard]] std::optional<ConnectRequest> takeHeld()
    {
        std::optional<ConnectRequest> request = m_held;
        m_held.reset();
        return request;
    }

    void dropHeld() { m_held.reset(); }

private:
    int  m_ticket = 0;
    bool m_commandRunning = false;
    std::optional<ConnectRequest> m_held;
};
