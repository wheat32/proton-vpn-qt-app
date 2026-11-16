// vpnManager.cpp
// VpnManager: construction, settings file access, and background status
// monitor infrastructure.
//
// All functions that interact with the protonvpn CLI live in protonvpnCli.cpp.

#include <QDir>
#include <QFile>
#include <QJsonArray>
// ReSharper disable once CppUnusedIncludeDirective
#include <QJsonDocument>
#include <QJsonObject>
#include "cli/cliSettings.h"
#include "cli/statusMonitor.h"
#include "cli/statusSnapshot.h"
#include "debug.h"
#include "uiHelpers.h"
#include "vpnManager.h"

namespace
{
// Path to the ProtonVPN settings file, relative to the user's home directory.
QString getSettingsFilePath()
{
    return QDir::homePath() + QStringLiteral("/.config/Proton/VPN/settings.json");
}

constexpr int KILLSWITCH_MODE_STANDARD    = 1;
constexpr int NETSHIELD_MODE_MALWARE      = 1;
constexpr int NETSHIELD_MODE_FULL         = 2;
} // namespace

VpnManager::VpnManager(QObject* parent)
    : QObject(parent),
      // A first answer for portForwardingEnabled() before the CLI is asked.
      m_settings(readSettingsFile())
{
}

// ---------------------------------------------------------------------------
// Settings file (no CLI involved; fetchSettings() is in protonvpnCli.cpp)
// ---------------------------------------------------------------------------

// static
QMap<QString, QString> VpnManager::readSettingsFile()
{
    QMap<QString, QString> settings;

    QFile f(getSettingsFilePath());
    if (f.open(QIODevice::ReadOnly) == false)
    {
        return settings;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    if (doc.isObject() == false)
    {
        return settings;
    }

    const QJsonObject root = doc.object();

    if (root.contains(QStringLiteral("killswitch")))
    {
        const int ks = root[QStringLiteral("killswitch")].toInt(0);
        settings.insert(QStringLiteral("kill-switch"),
                        ks == KILLSWITCH_MODE_STANDARD ? QStringLiteral("standard") : QStringLiteral("off"));
    }

    if (root.contains(QStringLiteral("ipv6")))
    {
        settings.insert(QStringLiteral("ipv6"),
                        onOffString(root[QStringLiteral("ipv6")].toBool()));
    }

    if (root.contains(QStringLiteral("anonymous_crash_reports")))
    {
        settings.insert(QStringLiteral("anonymous-crash-reports"),
                        onOffString(root[QStringLiteral("anonymous_crash_reports")].toBool()));
    }

    if (root.contains(QStringLiteral("custom_dns")))
    {
        const QJsonObject dns = root[QStringLiteral("custom_dns")].toObject();
        const bool dnsEnabled = dns[QStringLiteral("enabled")].toBool(false);
        if (dnsEnabled)
        {
            const QJsonArray ipList = dns[QStringLiteral("ip_list")].toArray();
            QStringList ips;
            for (const auto& v : ipList)
            {
                // The CLI saves each server as {"ip": "1.1.1.1", "enabled": true};
                // plain strings are accepted too.
                const QString ip = v.isObject() == true ? v.toObject().value(QStringLiteral("ip")).toString()
                                                        : v.toString();
                if (ip.isEmpty() == false)
                {
                    ips << ip;
                }
            }
            settings.insert(QStringLiteral("custom-dns"), CliSettings::customDnsValue(ips));
        }
        else
        {
            settings.insert(QStringLiteral("custom-dns"), QStringLiteral("off"));
        }
    }

    if (root.contains(QStringLiteral("features")))
    {
        const QJsonObject feat = root[QStringLiteral("features")].toObject();

        if (feat.contains(QStringLiteral("netshield")))
        {
            const int ns = feat[QStringLiteral("netshield")].toInt(0);
            QString nsVal;
            switch (ns)
            {
                case NETSHIELD_MODE_MALWARE:
                    nsVal = QStringLiteral("malware-only");
                    break;
                case NETSHIELD_MODE_FULL:
                    nsVal = QStringLiteral("malware-ads-trackers");
                    break;
                default:
                    nsVal = QStringLiteral("off");
                    break;
            }
            settings.insert(QStringLiteral("netshield"), nsVal);
        }

        if (feat.contains(QStringLiteral("moderate_nat")))
        {
            settings.insert(QStringLiteral("moderate-nat"),
                            onOffString(feat[QStringLiteral("moderate_nat")].toBool()));
        }

        if (feat.contains(QStringLiteral("vpn_accelerator")))
        {
            settings.insert(QStringLiteral("vpn-accelerator"),
                            onOffString(feat[QStringLiteral("vpn_accelerator")].toBool()));
        }

        if (feat.contains(QStringLiteral("port_forwarding")))
        {
            settings.insert(QStringLiteral("port-forwarding"),
                            onOffString(feat[QStringLiteral("port_forwarding")].toBool()));
        }
    }

    return settings;
}

bool VpnManager::portForwardingEnabled() const
{
    return isOnString(m_settings.value(QStringLiteral("port-forwarding")));
}

// ---------------------------------------------------------------------------
// Background status monitor (long-lived subprocess, every 15 s while logged in)
// ---------------------------------------------------------------------------

void VpnManager::startStatusMonitor()
{
    if (m_statusMonitor != nullptr) return;

    m_statusMonitor = new StatusMonitor(this);
    connect(m_statusMonitor, &StatusMonitor::statusParsed,
            this, [this](const QMap<QString, QString>& fields)
            {
                if (m_state == VpnState::Connecting || m_state == VpnState::Disconnecting) return; // ignore polls during in-progress transitions
                applyStatusFields(fields);
            });

    m_statusMonitor->start();
}

void VpnManager::stopStatusMonitor()
{
    if (m_statusMonitor == nullptr) return;

    m_statusMonitor->stop();
    m_statusMonitor->deleteLater();
    m_statusMonitor = nullptr;
}

// ---------------------------------------------------------------------------
// Apply a parsed `protonvpn status` snapshot to internal state.
// Only emits signals when state or connected server actually changed.
// ---------------------------------------------------------------------------

// static
QString VpnManager::stateToString(const VpnState state)
{
    switch (state)
    {
        case VpnState::Connected:
            return QStringLiteral("Connected");

        case VpnState::Disconnected:
            return QStringLiteral("Disconnected");

        case VpnState::Connecting:
            return QStringLiteral("Connecting");

        case VpnState::Disconnecting:
            return QStringLiteral("Disconnecting");

        case VpnState::Error:
            return QStringLiteral("Error");

        default:
            return QStringLiteral("Unknown");
    }
}


void VpnManager::applyStatusFields(const QMap<QString, QString>& fields)
{
    const VpnState prevState  = m_state;
    const QString  prevServer = m_connectedServer;

    const StatusSnapshot::Outcome outcome = StatusSnapshot::apply(m_state, m_connectedServer, fields);

    // A snapshot with no "status" field is not a disconnect - it is a snapshot
    // we could not read (empty output, a CLI traceback, a hung invocation).
    // Treating it as Disconnected would tear down a live session in the UI:
    // stop the elapsed timer, stop the port-forwarding keep-alive, fire a
    // spurious desktop notification, and potentially trigger auto-connect.
    // Ignore it and keep the last known state until a readable poll arrives.
    if (outcome.readable == false)
    {
        DBG_POLL(QStringLiteral("Snapshot had no status field - ignoring "
                                "(keeping state: %1).").arg(stateToString(m_state)));
        return;
    }

    m_state           = outcome.state;
    m_connectedServer = outcome.server;

    if (outcome.stateChanged == true || outcome.serverChanged == true)
    {
        if (m_state == VpnState::Connected)
        {
            const QString city = StatusMonitor::parseCityFromServer(m_connectedServer);
            if (city.isEmpty() == false)
            {
                emit connectionCityKnown(city);
            }
            // Country code - e.g. "US" from "US-NJ#203 in Secaucus, United States".
            const QString countryCode = StatusMonitor::parseCountryFromServer(m_connectedServer);
            if (countryCode.isEmpty() == false)
            {
                emit connectionCountryKnown(countryCode);
            }
            emit connectionStateChanged(m_state, m_connectedServer.isEmpty()
                                                     ? QString()
                                                     : QStringLiteral("Connected to %1.").arg(m_connectedServer));
        }
        else
        {
            emit connectionStateChanged(m_state, QString());
        }
    }
    else if (outcome.serverLearned == true)
    {
        // A connection the app made: the state was announced when the connect
        // finished, and this is the first word on which server it went to.
        // Only the country is passed on. The city stays what the user asked
        // for, so a "fastest server" choice is not turned into the city that
        // happened to be fastest this time.
        const QString countryCode = StatusMonitor::parseCountryFromServer(m_connectedServer);
        if (countryCode.isEmpty() == false)
        {
            emit connectionCountryKnown(countryCode);
        }
        DBG_POLL(QStringLiteral("Connected server is \"%1\".").arg(m_connectedServer));
    }

    if (outcome.stateChanged == true)
    {
        DBG_POLL(QStringLiteral("State changed:  %1 → %2")
                     .arg(stateToString(prevState), stateToString(m_state)));
    }
    else if (outcome.serverChanged == true)
    {
        DBG_POLL(QStringLiteral("Server changed: \"%1\" → \"%2\"")
                     .arg(prevServer, m_connectedServer));
    }
}

void VpnManager::announceState(const VpnState state, const QString& info)
{
    m_state = state;
    // Disconnecting still has the server it is leaving; every other state
    // but Connected has none.
    if (state != VpnState::Connected && state != VpnState::Disconnecting)
    {
        m_connectedServer.clear();
    }
    emit connectionStateChanged(m_state, info);
}

void VpnManager::refreshStatusNow()
{
    if (m_statusMonitor != nullptr)
    {
        m_statusMonitor->restart();
    }
}
