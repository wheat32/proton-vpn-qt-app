#pragma once

// ---------------------------------------------------------------------------
// makeStarButton – creates a small QToolButton that shows an empty or filled
// star depending on whether (countryCode, city) is in FavoritesManager.
// Clicking the button toggles the favorite state.
// The button also auto-updates when FavoritesManager::changed() fires.
// ---------------------------------------------------------------------------

#include <QCoreApplication>
#include <QIcon>
#include <QObject>
#include <QPainter>
#include <QPixmap>
#include <QToolButton>
#include "pixmapCache.h"
#include "../favoritesManager.h"
#include "../geoUtils.h"

namespace StarButtonConstants
{
constexpr int STAR_BTN_SIZE  = 22;
constexpr int STAR_ICON_SIZE = 13;
// The star sits in the top-left of an even-sized icon: in the even-sized
// button every Qt style then centers it at the same spot, where Breeze and
// Fusion (the AppImage's style) round an odd-sized icon's half pixel apart.
constexpr int STAR_ICON_BOX  = 14;

// Filled star: gold #FFD24A
constexpr int STAR_FILLED_R = 0xFF;
constexpr int STAR_FILLED_G = 0xD2;
constexpr int STAR_FILLED_B = 0x4A;

// Empty star: muted purple-grey #777799
constexpr int STAR_EMPTY_R = 0x77;
constexpr int STAR_EMPTY_G = 0x77;
constexpr int STAR_EMPTY_B = 0x99;
} // namespace StarButtonConstants

namespace StarButtonDetail
{
inline QIcon starIcon(const QString& resourcePath, const QColor& color)
{
    using namespace StarButtonConstants;

    const QPixmap star = GeoUtils::svgPixmap(resourcePath, STAR_ICON_SIZE, color);
    QPixmap box = blankPixmap(QSizeF(STAR_ICON_BOX, STAR_ICON_BOX), star.devicePixelRatio());
    QPainter painter(&box);
    painter.drawPixmap(0, 0, star);
    painter.end();
    return QIcon(box);
}
} // namespace StarButtonDetail

inline QToolButton* makeStarButton(const QString& countryCode,
                                    const QString& countryName,
                                    const QString& city,
                                    QWidget* parent = nullptr)
{
    using namespace StarButtonConstants;

    QToolButton* btn = new QToolButton(parent);
    btn->setFixedSize(STAR_BTN_SIZE, STAR_BTN_SIZE);
    btn->setIconSize(QSize(STAR_ICON_BOX, STAR_ICON_BOX));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setAutoRaise(true);
    btn->setObjectName(QStringLiteral("starButton"));
    // Base + hover styles are defined in style.qss / style_light.qss via QToolButton#starButton

    // Lambda that refreshes the button icon + tooltip.
    auto updateIcon = [btn, countryCode, city]()
    {
        const bool fav = FavoritesManager::instance().isFavorite(countryCode, city);
        if (fav == true)
        {
            btn->setIcon(StarButtonDetail::starIcon(QStringLiteral(":/assets/star-fill.svg"),
                                                    QColor(STAR_FILLED_R, STAR_FILLED_G, STAR_FILLED_B)));
            btn->setToolTip(QCoreApplication::translate("StarButton", "Remove from Favorites"));
        }
        else
        {
            btn->setIcon(StarButtonDetail::starIcon(QStringLiteral(":/assets/star.svg"),
                                                    QColor(STAR_EMPTY_R, STAR_EMPTY_G, STAR_EMPTY_B)));
            btn->setToolTip(QCoreApplication::translate("StarButton", "Add to Favorites"));
        }
    };

    updateIcon(); // set initial state

    QObject::connect(btn, &QToolButton::clicked, [countryCode, countryName, city]()
    {
        FavoritesManager::instance().toggle(countryCode, countryName, city);
    });

    // Keep icon in sync with any global favorites change.
    QObject::connect(&FavoritesManager::instance(), &FavoritesManager::changed,
                     btn, updateIcon);

    return btn;
}
