#pragma once

#include <QColor>
#include <QHeaderView>
#include <QPainter>
#include <QStyleOptionHeader>
#include "../geoUtils.h"
#include "../themeManager.h"

// A horizontal table header that draws its own sort arrow, a chevron at the
// right of the sorted column, instead of leaving it to the Qt style: Fusion
// (the AppImage's style) draws a triangle of its own whatever the stylesheet
// says, where Breeze draws the stylesheet's image.
class SortArrowHeader : public QHeaderView
{
    Q_OBJECT

    static constexpr int ARROW_SIZE = 10;
    // From the section's right edge, inside the room style.qss pads it with.
    static constexpr int ARROW_MARGIN = 8;
    // The headers' text colors in each theme.
    static constexpr QColor ARROW_ON_DARK{0x99, 0x99, 0xbb};
    static constexpr QColor ARROW_ON_LIGHT{0x5a, 0x5a, 0x7a};

public:
    explicit SortArrowHeader(QWidget* parent = nullptr)
        : QHeaderView(Qt::Horizontal, parent)
    {
    }

protected:
    void initStyleOptionForIndex(QStyleOptionHeader* option, const int logicalIndex) const override
    {
        QHeaderView::initStyleOptionForIndex(option, logicalIndex);
        // The style draws no arrow; paintSection() does.
        option->sortIndicator = QStyleOptionHeader::None;
    }

    void paintSection(QPainter* painter, const QRect& rect, const int logicalIndex) const override
    {
        QHeaderView::paintSection(painter, rect, logicalIndex);
        if (isSortIndicatorShown() == false || sortIndicatorSection() != logicalIndex) return;

        const QString icon = sortIndicatorOrder() == Qt::AscendingOrder
            ? QStringLiteral(":/assets/chevron-up.svg")
            : QStringLiteral(":/assets/chevron-down.svg");
        const QPixmap arrow = GeoUtils::svgPixmap(icon, ARROW_SIZE,
                                                  ThemeManager::isDark() == true ? ARROW_ON_DARK : ARROW_ON_LIGHT);
        painter->drawPixmap(rect.right() - ARROW_MARGIN - ARROW_SIZE,
                            rect.center().y() - ARROW_SIZE / 2, arrow);
    }
};
