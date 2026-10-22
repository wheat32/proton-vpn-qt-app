#pragma once

#include <QDialog>

class QLabel;
class QTableWidget;
class VpnManager;

// Everything known about a location: where it is, its features, whether it is
// a favorite and when it was last connected to, the live connection there,
// and each of its servers with its load, plan, features, status and
// addresses. The servers come from the CLI's server list cache, read in the
// background (see ServerListCache); without it the window shows the rest.
class ServerPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    // `city` is empty for the country as a whole, whose fastest server is
    // where a connection there goes.
    ServerPropertiesDialog(VpnManager* manager, const QString& countryCode, const QString& countryName,
                           const QString& city, QWidget* parent = nullptr);

private:
    VpnManager* m_manager;
    QString m_countryCode;
    QString m_countryName;
    QString m_city;

    QLabel* m_featuresValue = nullptr;
    QLabel* m_coordinatesValue = nullptr;
    QLabel* m_serversSummary = nullptr;
    QTableWidget* m_table = nullptr;

    // The server name connected to, when the connection is at this location.
    [[nodiscard]] QString connectedServerHere() const;
    // Fills in what the server list holds; `ok` is false when it could not
    // be read.
    void showServers(bool ok);
    void sizeColumns() const;
    // In verbose mode, prints every server and node shown (see VerboseLog).
    void logServers() const;
};
