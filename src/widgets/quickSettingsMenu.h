#pragma once

#include <QCoreApplication>
#include <QPoint>
#include <QString>

class QWidget;

// The right-click menu that puts a setting in the quick settings or takes it
// out, on the Settings page and in the quick settings themselves.
class QuickSettingsMenu
{
    Q_DECLARE_TR_FUNCTIONS(QuickSettingsMenu)

public:
    // Shows the menu for the setting `id` (see quickSettingIds.h) and saves
    // the user's choice to AppConfig. Returns true when the quick settings
    // changed.
    static bool exec(const QString& id, const QPoint& globalPos, QWidget* parent);
};
