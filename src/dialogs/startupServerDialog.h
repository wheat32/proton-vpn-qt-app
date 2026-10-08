#pragma once

#include <QDialog>
#include <QIcon>
#include <QString>

class VpnManager;

// Where to connect when the app starts (Settings > App > Auto-connect on
// Startup > Server): the Countries page's list in its choose mode, where the
// big button picks the selected country or city, and a Fastest Server button
// for the fastest server in any country.
class StartupServerDialog : public QDialog
{
    Q_OBJECT

public:
    // `serverKey` is the current choice, in AppConfig::autoConnectServer()'s
    // format.
    StartupServerDialog(VpnManager* manager, const QString& serverKey, QWidget* parent = nullptr);

    // The choice, in the same format; set once the dialog is accepted.
    [[nodiscard]] QString serverKey() const { return m_serverKey; }

    // A choice by name ("Fastest Server", "Fastest in Sweden",
    // "United States, New York"), and its flag (none for the fastest server
    // anywhere).
    static QString displayName(const QString& serverKey);
    static QIcon icon(const QString& serverKey);

private:
    QString m_serverKey;
};
