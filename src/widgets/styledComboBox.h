#pragma once

#include <QComboBox>
#include <QStyledItemDelegate>

// A combo box whose popup items are drawn by the stylesheet, so the dropdown
// looks the same under every Qt style. Breeze replaces a combo box's default
// item delegate with its own, which draws the popup items itself and ignores
// the stylesheet, and Fusion (the AppImage's style) draws them differently
// again. Breeze leaves a QStyledItemDelegate alone, and it draws items with
// the stylesheet's "QComboBox QAbstractItemView::item" rules under every style.
class StyledComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit StyledComboBox(QWidget* parent = nullptr)
        : QComboBox(parent)
    {
        setItemDelegate(new QStyledItemDelegate(this));
    }
};
