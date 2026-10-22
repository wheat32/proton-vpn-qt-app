#pragma once

#include <QCoreApplication>
#include <QPoint>
#include <QString>

class QWidget;
class VpnManager;

// The right-click menu for a location in the Countries list or the VPN
// page's dropdowns: add it to Favorites or take it out, or show its
// Properties (see ServerPropertiesDialog).
class ServerContextMenu
{
    Q_DECLARE_TR_FUNCTIONS(ServerContextMenu)

public:
    // `city` is empty for the country's fastest server. The menu and the
    // Properties window open over `window`: parented inside the VPN page,
    // its stylesheet would clear their backgrounds. For a location in an
    // open dropdown, `popup` is the dropdown's popup window: the menu opens
    // over it, which stays open, unless Properties is chosen, as the window
    // could not take input under it.
    static void exec(VpnManager* manager, const QString& countryCode, const QString& countryName,
                     const QString& city, const QPoint& globalPos, QWidget* window,
                     QWidget* popup = nullptr);
};
