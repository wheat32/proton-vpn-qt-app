#pragma once

#include <QComboBox>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

class ToggleWithStatus;
class VpnManager;

// Ties toggles and combo boxes to the VPN settings the CLI keeps (`protonvpn
// config`), for the Settings page and the VPN page's quick settings. Each
// control shows its setting as VpnManager last read it and follows every new
// read. The user's changes go through VpnManager; a setting that only takes
// effect on a new connection first asks, unless the VPN is disconnected,
// whether to reconnect for it, and the control goes back to the current
// setting if the user dismisses that.
class CliSettingControls : public QObject
{
    Q_OBJECT

public:
    // The reconnect question opens over `parent`'s window; `parent` also owns
    // this object.
    CliSettingControls(VpnManager* manager, QWidget* parent);

    // `onValue` is what the CLI takes and reports for on ("standard" for the
    // kill switch); off is "off". `label` names the setting in the reconnect
    // question.
    void addToggle(ToggleWithStatus* toggle, const QString& label, const QString& key,
                   const QString& onValue = QStringLiteral("on"), bool requiresReconnect = false);
    // `values` are the CLI values of the combo box's items, in order.
    void addCombo(QComboBox* combo, const QString& label, const QString& key,
                  const QStringList& values, bool requiresReconnect = false);

    void setEnabled(bool enabled) const;

signals:
    // The user chose Apply & Reconnect: the VPN now disconnects, the change is
    // applied, and it connects again.
    void reconnectStarted();

private:
    struct Toggle
    {
        ToggleWithStatus* toggle = nullptr;
        QString key;
        QString onValue;
    };

    struct Combo
    {
        QComboBox* combo = nullptr;
        QString key;
        QStringList values;
    };

    VpnManager* m_manager;
    QWidget* m_parentWidget;
    QList<Toggle> m_toggles;
    QList<Combo> m_combos;

    static void show(const Toggle& toggle, const QMap<QString, QString>& settings);
    static void show(const Combo& combo, const QMap<QString, QString>& settings);
    void showAll(const QMap<QString, QString>& settings) const;
    // Asks whether to reconnect for the change to `label`; true to go ahead.
    [[nodiscard]] bool confirmReconnect(const QString& label) const;
    // Whether changing a setting now has to go through the reconnect question.
    [[nodiscard]] bool needsReconnect(bool requiresReconnect) const;
    void reconnect(const QString& key, const QString& value);
};
