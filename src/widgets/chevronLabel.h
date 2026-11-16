#pragma once

// chevronLabel.h
// The small chevron a collapsible header shows: down while collapsed, up
// while expanded, drawn in the theme's icon color.

#include <QColor>
#include <QEvent>
#include <QLabel>
#include "../geoUtils.h"
#include "../themeManager.h"

namespace ChevronLabelDetail
{
constexpr int ICON_SIZE = 12;
constexpr QColor COLOR_ON_DARK(200, 200, 220);
constexpr QColor COLOR_ON_LIGHT(90, 90, 122);
} // namespace ChevronLabelDetail

class ChevronLabel : public QLabel
{
public:
    explicit ChevronLabel(QWidget* parent = nullptr)
        : QLabel(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        refresh();
    }

    void setExpanded(const bool expanded)
    {
        if (m_expanded == expanded) return;
        m_expanded = expanded;
        refresh();
    }

protected:
    void changeEvent(QEvent* event) override
    {
        QLabel::changeEvent(event);
        if (event->type() == QEvent::PaletteChange)
        {
            refresh();
        }
    }

private:
    void refresh()
    {
        const QString path = m_expanded == true ? QStringLiteral(":/assets/chevron-up.svg")
                                                : QStringLiteral(":/assets/chevron-down.svg");
        const QColor color = ThemeManager::isDark() == true ? ChevronLabelDetail::COLOR_ON_DARK
                                                            : ChevronLabelDetail::COLOR_ON_LIGHT;
        setPixmap(GeoUtils::svgPixmap(path, ChevronLabelDetail::ICON_SIZE, color));
    }

    bool m_expanded = false;
};
