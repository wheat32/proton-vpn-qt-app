#include "serverContextMenu.h"
#include "../appConfig.h"
#include "../favoritesManager.h"
#include "../dialogs/serverPropertiesDialog.h"

#include <QAction>
#include <QEvent>
#include <QMenu>
#include <QMouseEvent>
#include <optional>

namespace
{
// Where the click that closed the menu landed, when one outside it did.
class ClosingClickWatcher : public QObject
{
public:
    std::optional<QPoint> globalPos;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            const QMouseEvent* mouse = static_cast<const QMouseEvent*>(event);
            const QWidget* menu = static_cast<const QWidget*>(watched);
            if (menu->rect().contains(mouse->position().toPoint()) == false)
            {
                globalPos = mouse->globalPosition().toPoint();
            }
        }
        return false;
    }
};
} // namespace

// static
void ServerContextMenu::exec(VpnManager* manager, const QString& countryCode, const QString& countryName,
                             const QString& city, const QPoint& globalPos, QWidget* window,
                             QWidget* popup)
{
    // Over a popup, as its child: on Wayland a popup opened over another
    // has to name it as its parent.
    QMenu menu(popup != nullptr ? popup : window);
    // Offered wherever the star buttons are: whenever Favorites are on.
    QAction* favoriteAction = nullptr;
    if (AppConfig::instance().favoritesEnabled() == true)
    {
        favoriteAction = menu.addAction(FavoritesManager::instance().isFavorite(countryCode, city) == true
                                            ? tr("Remove from Favorites")
                                            : tr("Add to Favorites"));
        menu.addSeparator();
    }
    QAction* propertiesAction = menu.addAction(tr("Properties…"));

    ClosingClickWatcher closingClick;
    menu.installEventFilter(&closingClick);
    const QAction* chosen = menu.exec(globalPos);
    if (chosen == nullptr)
    {
        // Closed by a click outside the dropdown as well: that closes the
        // dropdown too, as it would have without the menu.
        if (popup != nullptr && closingClick.globalPos.has_value() == true
            && popup->frameGeometry().contains(*closingClick.globalPos) == false)
        {
            popup->hide();
        }
        return;
    }
    if (chosen == favoriteAction)
    {
        FavoritesManager::instance().toggle(countryCode, countryName, city);
    }
    else if (chosen == propertiesAction)
    {
        if (popup != nullptr)
        {
            popup->hide();
        }
        ServerPropertiesDialog dialog(manager, countryCode, countryName, city, window);
        dialog.exec();
    }
}
