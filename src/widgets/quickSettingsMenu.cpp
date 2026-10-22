#include "quickSettingsMenu.h"
#include "../appConfig.h"

#include <QAction>
#include <QMenu>
#include <QStringList>

// static
bool QuickSettingsMenu::exec(const QString& id, const QPoint& globalPos, QWidget* parent)
{
    QStringList chosen = AppConfig::instance().quickSettings();
    const bool shown = chosen.contains(id);
    const bool full = shown == false && chosen.size() >= AppConfig::QUICK_SETTINGS_MAX;

    QMenu menu(parent);
    QAction* showAction = menu.addAction(tr("Show in Quick Settings"));
    showAction->setCheckable(true);
    showAction->setChecked(shown);
    showAction->setEnabled(full == false);
    if (full == true)
    {
        QAction* fullNote = menu.addAction(tr("Quick settings are full (%1 of %1). Take one out first.")
                                               .arg(AppConfig::QUICK_SETTINGS_MAX));
        fullNote->setEnabled(false);
    }
    if (menu.exec(globalPos) != showAction) return false;

    if (shown == true)
    {
        chosen.removeAll(id);
    }
    else
    {
        chosen.append(id);
    }
    AppConfig::instance().setQuickSettings(chosen);
    return true;
}
