#include <QColor>
#include <QEasingCurve>
#include "../connectionHistory.h"
#include "../favoritesManager.h"
#include "pickerDrawer.h"

namespace
{
constexpr int SHADOW_BLUR_RADIUS        = 20;
constexpr int SHADOW_OFFSET_X           = 6;
constexpr int SHADOW_ALPHA              = 130;
constexpr int DRAWER_V_MARGIN           = 20;
constexpr int DRAWER_SPACING            = 8;
constexpr int ANIM_DURATION_MS          = 220;
constexpr int DRAWER_EXPANDED_H_MARGIN  = 16;
constexpr int DRAWER_COLLAPSED_R_MARGIN = 2;
} // namespace

PickerDrawer::PickerDrawer(PickerBase* loc, PickerBase* rec,
                           PickerBase* fav, QWidget* parent)
    : QFrame(parent), m_locationPicker(loc), m_recentPicker(rec), m_favoritesPicker(fav)
{
    setObjectName(QStringLiteral("pickerDrawer"));
    setAttribute(Qt::WA_StyledBackground);

    m_shadow = new QGraphicsDropShadowEffect(this);
    m_shadow->setBlurRadius(SHADOW_BLUR_RADIUS);
    m_shadow->setOffset(SHADOW_OFFSET_X, 0);
    m_shadow->setColor(QColor(0, 0, 0, SHADOW_ALPHA));
    m_shadow->setEnabled(false); // only enabled when expanded
    setGraphicsEffect(m_shadow);

    m_contentLayout = new QVBoxLayout(this);
    // SetNoConstraint: lets the drawer resize freely even when pickers are larger.
    m_contentLayout->setSizeConstraint(QLayout::SetNoConstraint);
    m_contentLayout->setContentsMargins(0, DRAWER_V_MARGIN, 0, DRAWER_V_MARGIN);
    m_contentLayout->setSpacing(DRAWER_SPACING);

    // Re-parent pickers into the drawer
    m_locationPicker->setParent(this);
    m_recentPicker->setParent(this);
    if (m_favoritesPicker != nullptr)
    {
        m_favoritesPicker->setParent(this);
    }

    m_contentLayout->addWidget(m_locationPicker);
    m_contentLayout->setAlignment(m_locationPicker, Qt::AlignHCenter);
    m_contentLayout->addWidget(m_recentPicker);
    m_contentLayout->setAlignment(m_recentPicker, Qt::AlignHCenter);
    if (m_favoritesPicker != nullptr)
    {
        m_contentLayout->addWidget(m_favoritesPicker);
        m_contentLayout->setAlignment(m_favoritesPicker, Qt::AlignHCenter);
    }

    setAllCollapsed(true);
    setDrawerW(COLLAPSED_DRAWER_WIDTH);
}

void PickerDrawer::setDrawerW(int w)
{
    QWidget::setMinimumWidth(w);
    QWidget::setMaximumWidth(w);
    emit drawerWidthChanged(w);
}

void PickerDrawer::toggle()
{
    if (m_anim != nullptr)
    {
        m_anim->stop(); // triggers destroyed -> m_anim = nullptr
    }

    const int fromW = m_expanded ? EXPANDED_DRAWER_WIDTH : COLLAPSED_DRAWER_WIDTH;
    const int toW   = m_expanded ? COLLAPSED_DRAWER_WIDTH : EXPANDED_DRAWER_WIDTH;
    m_expanded = (m_expanded == false);

    if (m_expanded == false)
    {
        // Collapsing: shrink content immediately, then slide background in.
        setAllCollapsed(true);
        if (m_shadow != nullptr)
        {
            m_shadow->setEnabled(false);
        }
    }

    m_anim = new QPropertyAnimation(this, "drawerW", this);
    m_anim->setStartValue(fromW);
    m_anim->setEndValue(toW);
    m_anim->setDuration(ANIM_DURATION_MS);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);

    if (m_expanded == true)
    {
        // Expanding: reveal full content only after slide animation completes.
        connect(m_anim, &QPropertyAnimation::finished, this, [this]()
        {
            setAllCollapsed(false);
            if (m_shadow != nullptr)
            {
                m_shadow->setEnabled(true);
            }
        });
    }

    connect(m_anim, &QPropertyAnimation::destroyed, this, [this]()
    {
        m_anim = nullptr;
    });

    m_anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void PickerDrawer::setAllCollapsed(const bool c) const
{
    // Do not touch collapse state while pickers live in the wide-mode sidebar.
    if (m_pickersReleased == true)
    {
        return;
    }

    m_locationPicker->setCollapsed(c);
    if (m_recentPicker->isVisible() == true)
    {
        m_recentPicker->setCollapsed(c);
    }
    if (m_favoritesPicker != nullptr && m_favoritesPicker->isVisible() == true)
    {
        m_favoritesPicker->setCollapsed(c);
    }

    m_contentLayout->setContentsMargins(
        c == true ? 0 : DRAWER_EXPANDED_H_MARGIN,
        DRAWER_V_MARGIN,
        c == true ? DRAWER_COLLAPSED_R_MARGIN : DRAWER_EXPANDED_H_MARGIN,
        DRAWER_V_MARGIN);
}

void PickerDrawer::syncVisibility(const bool isFreeUser,
    const bool showFavDropdown,
    const bool favEnabled)
{
    const bool hasHistory   = (isFreeUser == false)
                              && (ConnectionHistory::instance().entries().isEmpty() == false);
    const bool hasFavorites = (isFreeUser == false) && (favEnabled == true)
                              && (showFavDropdown == true)
                              && FavoritesManager::instance().hasAnyEntries();

    m_recentPicker->setVisible(hasHistory);
    if (m_favoritesPicker != nullptr)
    {
        m_favoritesPicker->setVisible(hasFavorites);
    }

    // Do not override collapsed state when pickers live in the wide-mode sidebar.
    if (m_pickersReleased == false)
    {
        if (m_expanded == false)
        {
            if (hasHistory == true)
            {
                m_recentPicker->setCollapsed(true);
            }
            if (hasFavorites == true && m_favoritesPicker != nullptr)
            {
                m_favoritesPicker->setCollapsed(true);
            }
        }
        else
        {
            if (hasHistory == true)
            {
                m_recentPicker->setCollapsed(false);
            }
            if (hasFavorites == true && m_favoritesPicker != nullptr)
            {
                m_favoritesPicker->setCollapsed(false);
            }
        }
    }

    notifyAvailability();
}

bool PickerDrawer::hasAnyVisiblePicker() const
{
    // Use isHidden() so the check reflects each widget's own show/hide state,
    // independent of whether the drawer parent is currently hidden.
    if (m_locationPicker != nullptr && m_locationPicker->isHidden() == false)
    {
        return true;
    }
    if (m_recentPicker != nullptr && m_recentPicker->isHidden() == false)
    {
        return true;
    }
    if (m_favoritesPicker != nullptr && m_favoritesPicker->isHidden() == false)
    {
        return true;
    }
    return false;
}

void PickerDrawer::notifyAvailability()
{
    emit pickerAvailabilityChanged(hasAnyVisiblePicker());
}

void PickerDrawer::releasePickers()
{
    m_pickersReleased = true;
    m_contentLayout->removeWidget(m_locationPicker);
    m_contentLayout->removeWidget(m_recentPicker);
    if (m_favoritesPicker != nullptr)
    {
        m_contentLayout->removeWidget(m_favoritesPicker);
    }
}

void PickerDrawer::reclaimPickers()
{
    m_pickersReleased = false;
    m_contentLayout->addWidget(m_locationPicker);
    m_contentLayout->setAlignment(m_locationPicker, Qt::AlignHCenter);
    m_contentLayout->addWidget(m_recentPicker);
    m_contentLayout->setAlignment(m_recentPicker, Qt::AlignHCenter);
    if (m_favoritesPicker != nullptr)
    {
        m_contentLayout->addWidget(m_favoritesPicker);
        m_contentLayout->setAlignment(m_favoritesPicker, Qt::AlignHCenter);
    }
}
