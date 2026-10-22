#include "cliSettingControls.h"
#include "toggleWithStatus.h"
#include "../uiHelpers.h"
#include "../vpnManager.h"
#include "../dialogs/reconnectDialog.h"

#include <QSignalBlocker>
#include <algorithm>

CliSettingControls::CliSettingControls(VpnManager* manager, QWidget* parent)
    : QObject(parent), m_manager(manager), m_parentWidget(parent)
{
    connect(m_manager, &VpnManager::settingsReady, this, &CliSettingControls::showAll);
}

void CliSettingControls::addToggle(ToggleWithStatus* toggle, const QString& label, const QString& key,
                                   const QString& onValue, const bool requiresReconnect)
{
    const Toggle entry{.toggle = toggle, .key = key, .onValue = onValue};
    m_toggles.append(entry);
    show(entry, m_manager->settings());

    connect(toggle, &ToggleWithStatus::toggled, this, [this, entry, label, requiresReconnect](const bool on)
    {
        const QString value = on == true ? entry.onValue : QStringLiteral("off");
        if (needsReconnect(requiresReconnect) == true)
        {
            // Unchanged unless the user goes ahead.
            show(entry, m_manager->settings());
            if (confirmReconnect(label) == false) return;
            entry.toggle->setOn(on, true);
            reconnect(entry.key, value);
            return;
        }
        m_manager->applyConfigValue(entry.key, value);
    });
}

void CliSettingControls::addCombo(QComboBox* combo, const QString& label, const QString& key,
                                  const QStringList& values, const bool requiresReconnect)
{
    const Combo entry{.combo = combo, .key = key, .values = values};
    m_combos.append(entry);
    show(entry, m_manager->settings());

    connect(combo, &QComboBox::currentIndexChanged, this, [this, entry, label, requiresReconnect](const int idx)
    {
        if (idx < 0 || idx >= entry.values.size()) return;
        const QString value = entry.values.at(idx);
        if (needsReconnect(requiresReconnect) == true)
        {
            // Unchanged unless the user goes ahead.
            show(entry, m_manager->settings());
            if (confirmReconnect(label) == false) return;
            const QSignalBlocker blocker(entry.combo);
            entry.combo->setCurrentIndex(idx);
            reconnect(entry.key, value);
            return;
        }
        m_manager->applyConfigValue(entry.key, value);
    });
}

void CliSettingControls::setEnabled(const bool enabled) const
{
    for (const Toggle& t : m_toggles)
    {
        t.toggle->setEnabled(enabled);
    }
    for (const Combo& c : m_combos)
    {
        c.combo->setEnabled(enabled);
    }
}

// static
void CliSettingControls::show(const Toggle& toggle, const QMap<QString, QString>& settings)
{
    const QString value = settings.value(toggle.key).toLower().trimmed();
    toggle.toggle->setOn(value == toggle.onValue || isOnString(value), false);
}

// static
void CliSettingControls::show(const Combo& combo, const QMap<QString, QString>& settings)
{
    // An unknown value shows as the first item.
    const qsizetype idx = combo.values.indexOf(settings.value(combo.key).toLower().trimmed());
    const QSignalBlocker blocker(combo.combo);
    combo.combo->setCurrentIndex(static_cast<int>(std::max<qsizetype>(idx, 0)));
}

void CliSettingControls::showAll(const QMap<QString, QString>& settings) const
{
    for (const Toggle& t : m_toggles)
    {
        show(t, settings);
    }
    for (const Combo& c : m_combos)
    {
        show(c, settings);
    }
}

bool CliSettingControls::confirmReconnect(const QString& label) const
{
    // Over the window rather than the parent itself: the VPN page's
    // stylesheet clears the background of every widget inside it, and a
    // dialog parented in there would show black.
    return ReconnectDialog::confirm(label, m_parentWidget->window());
}

bool CliSettingControls::needsReconnect(const bool requiresReconnect) const
{
    return requiresReconnect == true && m_manager->currentState() != VpnState::Disconnected;
}

void CliSettingControls::reconnect(const QString& key, const QString& value)
{
    // Before the call: it reports Disconnecting straight away.
    emit reconnectStarted();
    m_manager->applyConfigValueAndReconnect(key, value);
}
