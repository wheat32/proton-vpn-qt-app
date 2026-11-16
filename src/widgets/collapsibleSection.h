#pragma once

// collapsibleSection.h
// A titled section that keeps its content hidden until its header is clicked.
// Used for the settings most people never need ("Advanced").

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMargins>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QWidget>
#include "chevronLabel.h"

class CollapsibleSection : public QWidget
{
    Q_OBJECT

public:
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        QVBoxLayout* outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        m_header = new QWidget(this);
        m_header->setCursor(Qt::PointingHandCursor);
        m_header->setFocusPolicy(Qt::TabFocus);
        m_header->installEventFilter(this);
        m_headerLayout = new QHBoxLayout(m_header);
        QLabel* titleLabel = new QLabel(title.toUpper(), m_header);
        titleLabel->setObjectName(QStringLiteral("appSectionHeader"));
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_headerLayout->addWidget(titleLabel);
        m_headerLayout->addStretch();
        m_chevron = new ChevronLabel(m_header);
        m_headerLayout->addWidget(m_chevron);
        outer->addWidget(m_header);

        m_content = new QWidget(this);
        m_contentLayout = new QVBoxLayout(m_content);
        m_contentLayout->setContentsMargins(0, 0, 0, 0);
        m_contentLayout->setSpacing(0);
        m_content->setVisible(false);
        outer->addWidget(m_content);
    }

    // Where the section's widgets go.
    [[nodiscard]] QWidget* content() const { return m_content; }
    [[nodiscard]] QVBoxLayout* contentLayout() const { return m_contentLayout; }

    void setHeaderMargins(const QMargins& margins) const { m_headerLayout->setContentsMargins(margins); }

    [[nodiscard]] bool isExpanded() const { return m_expanded; }

    void setExpanded(const bool expanded)
    {
        if (m_expanded == expanded) return;
        m_expanded = expanded;
        m_content->setVisible(expanded);
        m_chevron->setExpanded(expanded);
        emit expandedChanged(expanded);
    }

signals:
    void expandedChanged(bool expanded);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_header)
        {
            if (event->type() == QEvent::MouseButtonRelease
                && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton)
            {
                setExpanded(m_expanded == false);
                return true;
            }
            if (event->type() == QEvent::KeyPress)
            {
                const int key = static_cast<QKeyEvent*>(event)->key();
                if (key == Qt::Key_Space || key == Qt::Key_Return || key == Qt::Key_Enter)
                {
                    setExpanded(m_expanded == false);
                    return true;
                }
            }
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    QWidget*      m_header        = nullptr;
    QHBoxLayout*  m_headerLayout  = nullptr;
    ChevronLabel* m_chevron       = nullptr;
    QWidget*      m_content       = nullptr;
    QVBoxLayout*  m_contentLayout = nullptr;
    bool          m_expanded      = false;
};
