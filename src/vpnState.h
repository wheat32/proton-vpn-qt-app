#pragma once

// vpnState.h
// The connection states the app works with. In its own header so the pure
// helpers that decide state changes (cli/statusSnapshot.h) can be unit tested
// without pulling in VpnManager.

enum class VpnState
{
    Unknown,
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};
