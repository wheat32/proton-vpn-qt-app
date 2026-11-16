#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QWidget>
#include "chevronLabel.h"

// "Available to Plus Members" between two lines, shown to free accounts above
// the settings only Plus members can change. Those settings are of no use to a
// free account, so the divider is also their header: they stay folded away
// until it is clicked.
class PlusDivider : public QWidget
{
    static constexpr int H_MARGIN = 16;
    static constexpr int V_MARGIN = 10;
    static constexpr int SPACING  = 10;

    Q_OBJECT

public:
    explicit PlusDivider(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);

        QHBoxLayout* hl = new QHBoxLayout(this);
        hl->setContentsMargins(H_MARGIN, V_MARGIN, H_MARGIN, V_MARGIN);
        hl->setSpacing(SPACING);

        hl->addWidget(makeLine(), 1);

        QLabel* label = new QLabel(tr("✦  Available to Plus Members"), this);
        label->setObjectName(QStringLiteral("plusDividerLabel"));
        label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        hl->addWidget(label);

        m_chevron = new ChevronLabel(this);
        hl->addWidget(m_chevron);

        hl->addWidget(makeLine(), 1);
    }

    [[nodiscard]] bool isExpanded() const { return m_expanded; }

    void setExpanded(const bool expanded)
    {
        if (m_expanded == expanded) return;
        m_expanded = expanded;
        m_chevron->setExpanded(expanded);
        emit expandedChanged(expanded);
    }

signals:
    void expandedChanged(bool expanded);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            setExpanded(m_expanded == false);
        }
        QWidget::mouseReleaseEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        const int key = event->key();
        if (key == Qt::Key_Space || key == Qt::Key_Return || key == Qt::Key_Enter)
        {
            setExpanded(m_expanded == false);
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    QFrame* makeLine()
    {
        QFrame* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setObjectName(QStringLiteral("plusDividerLine"));
        line->setAttribute(Qt::WA_TransparentForMouseEvents);
        return line;
    }

    ChevronLabel* m_chevron  = nullptr;
    bool          m_expanded = false;
};
