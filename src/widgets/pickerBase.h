#pragma once

#include <QAbstractButton>
#include <QContextMenuEvent>
#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QListWidget>
#include <QMouseEvent>
#include <QPointer>
#include <QScreen>
#include <QScrollBar>
#include <QVBoxLayout>
#include <algorithm>
#include "elidaLabel.h"
#include "styleUtils.h"

// ============================================================
// PickerBase – shared base for LocationPicker and RecentPicker.
//
// Provides:
//   • The floating Qt::Popup frame + QListWidget plumbing.
//   • installOnRowWidget() - recursively enables hover tracking.
//   • Row hover / leave event-filter logic.
//   • togglePopup() / closePopup() / resizeList() helpers; the popup is
//     sized to its rows' full text.
// ============================================================
class PickerBase : public QFrame
{
    Q_OBJECT
public:
    static constexpr int COLLAPSED_PICKER_WIDTH  = 48;
    static constexpr int EXPANDED_PICKER_WIDTH   = 260;
    static constexpr int PICKER_MARGIN           = 10;
    static constexpr int PICKER_EXPANDED_V_MARGIN = 8;
    static constexpr int POPUP_MAX_VISIBLE_ROWS  = 8;
    static constexpr int POPUP_LIST_BORDER       = 2;
    // Leading icon slot in the header (flag, clock, star) - sized 4:3 so flags
    // keep their natural aspect ratio.
    static constexpr int LEADING_ICON_W          = 28;
    static constexpr int LEADING_ICON_H          = 21;
    static constexpr int TEXT_COL_SPACING        = 1;

    explicit PickerBase(QWidget* parent = nullptr) : QFrame(parent) {}

    // Switches between compact icon-only (collapsed) and full label (expanded) mode.
    void setCollapsed(bool c)
    {
        m_collapsed = c;
        if (m_topLine != nullptr)
        {
            m_topLine->setVisible(c == false);
        }
        if (m_bottomLine != nullptr)
        {
            m_bottomLine->setVisible(c == false);
        }
        if (m_chevron != nullptr)
        {
            m_chevron->setVisible(c == false && m_list != nullptr && m_list->count() > 0);
        }
        if (m_header != nullptr)
        {
            QHBoxLayout* hl = qobject_cast<QHBoxLayout*>(m_header->layout());
            if (hl != nullptr)
            {
                // Collapsed: 16 px each side of the 28 px icon fills the full 60 px button.
                // No inter-item spacing so hidden stretch items steal nothing.
                const int vMargin = (c == true) ? PICKER_MARGIN : PICKER_EXPANDED_V_MARGIN;
                const int spacing = (c == true) ? 0 : PICKER_MARGIN;
                hl->setContentsMargins(PICKER_MARGIN, vMargin, PICKER_MARGIN, vMargin);
                hl->setSpacing(spacing);
            }
        }
        setFixedWidth(c == true ? COLLAPSED_PICKER_WIDTH : EXPANDED_PICKER_WIDTH);
    }

signals:
    // A row that stands for a location was right-clicked. The popup stays
    // open: its menu opens over it (see popupWindow()). `city` is empty for
    // the country's fastest server.
    void rowContextMenuRequested(const QString& countryCode, const QString& countryName,
                                 const QString& city, const QPoint& globalPos);

public:
    // The dropdown's popup window, for a row's menu to open over (as its
    // child) and to close before a window the menu leads to, which cannot
    // take input under an open popup.
    [[nodiscard]] QWidget* popupWindow() const
    {
        return m_popup;
    }

    // The picker's height when expanded, also while it is collapsed, so the
    // page can tell whether the expanded pickers would fit before moving them.
    [[nodiscard]] int expandedHeight() const
    {
        if (m_collapsed == false || m_topLine == nullptr || m_bottomLine == nullptr)
        {
            return sizeHint().height();
        }
        // Collapsed, the header holds only the leading icon between
        // PICKER_MARGIN margins; expanded, the taller of the icon and the two
        // text lines between PICKER_EXPANDED_V_MARGIN ones.
        const int textH = m_topLine->sizeHint().height() + TEXT_COL_SPACING + m_bottomLine->sizeHint().height();
        return sizeHint().height() - 2 * PICKER_MARGIN - LEADING_ICON_H
             + 2 * PICKER_EXPANDED_V_MARGIN + std::max(LEADING_ICON_H, textH);
    }

protected:
    // Builds the standard picker header - leading icon, two-line text block,
    // chevron - plus the outer layout, the popup and the row-click wiring.
    //
    // All three pickers had their own copy of this; they differed only in the
    // leading icon and the two strings, so any styling change had to be made
    // three times.  `leadingIcon` is reparented into the header (pass nullptr
    // for no icon).
    void buildHeader(QWidget* leadingIcon, const QString& topText, const QString& bottomText)
    {
        m_header = new QFrame(this);
        m_header->setObjectName(QStringLiteral("locationPickerHeader"));
        m_header->setCursor(Qt::PointingHandCursor);

        QHBoxLayout* headerLayout = new QHBoxLayout(m_header);
        headerLayout->setContentsMargins(PICKER_MARGIN, PICKER_EXPANDED_V_MARGIN,
                                         PICKER_MARGIN, PICKER_EXPANDED_V_MARGIN);
        headerLayout->setSpacing(PICKER_MARGIN);

        if (leadingIcon != nullptr)
        {
            leadingIcon->setParent(m_header);
            leadingIcon->setFixedSize(LEADING_ICON_W, LEADING_ICON_H);
            QLabel* iconLabel = qobject_cast<QLabel*>(leadingIcon);
            if (iconLabel != nullptr)
            {
                iconLabel->setAlignment(Qt::AlignCenter);
            }
            headerLayout->addWidget(leadingIcon);
        }

        QVBoxLayout* textCol = new QVBoxLayout();
        textCol->setSpacing(TEXT_COL_SPACING);
        textCol->setContentsMargins(0, 0, 0, 0);

        m_topLine = new ElideLabel(topText, m_header);
        m_topLine->setObjectName(QStringLiteral("locationPickerTop"));
        m_bottomLine = new ElideLabel(bottomText, m_header);
        m_bottomLine->setObjectName(QStringLiteral("locationPickerBottom"));
        textCol->addWidget(m_topLine);
        textCol->addWidget(m_bottomLine);
        headerLayout->addLayout(textCol, 1);

        m_chevron = new QLabel(QStringLiteral("▾"), m_header);
        m_chevron->setObjectName(QStringLiteral("locationPickerChevron"));
        headerLayout->addWidget(m_chevron);

        QVBoxLayout* outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        outerLayout->setSpacing(0);
        outerLayout->addWidget(m_header);

        initPopup();
        m_header->installEventFilter(this);
        // Routed through the virtual so the base class does not need to know
        // which subclass it is building for.
        connect(m_list, &QListWidget::itemClicked, this,
                [this](QListWidgetItem* item) { onRowClicked(item); });
    }

    // Must be called by the subclass constructor after building the header.
    void initPopup()
    {
        m_popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
        m_popup->setObjectName(QStringLiteral("locationPickerPopup"));

        QVBoxLayout* popupLayout = new QVBoxLayout(m_popup);
        popupLayout->setContentsMargins(0, 0, 0, 0);
        popupLayout->setSpacing(0);

        m_list = new QListWidget(m_popup);
        m_list->setObjectName(QStringLiteral("locationPickerList"));
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_list->setCursor(Qt::PointingHandCursor);
        m_list->setMouseTracking(true);
        m_list->viewport()->setMouseTracking(true);
        m_list->viewport()->installEventFilter(this);
        popupLayout->addWidget(m_list);

        m_popup->installEventFilter(this);
    }

    void togglePopup() const
    {
        if (m_list->count() == 0) return;
        if (m_popup->isVisible())
        {
            closePopup();
            return;
        }
        resizeList();
        m_popup->show();
        if (m_chevron != nullptr)
        {
            m_chevron->setText(QStringLiteral("▴"));
        }
    }

    void closePopup() const
    {
        m_popup->hide();
        if (m_chevron != nullptr)
        {
            m_chevron->setText(QStringLiteral("▾"));
        }
    }

    // Fits the popup to its rows - up to POPUP_MAX_VISIBLE_ROWS tall, and wide
    // enough for every row's text (see popupWidth()) - and places it under
    // the picker, kept on screen.
    void resizeList() const
    {
        const int count = m_list->count();
        if (count == 0) return;
        const int rowH  = m_list->sizeHintForRow(0);
        const int listH = qMin(count, POPUP_MAX_VISIBLE_ROWS) * rowH + POPUP_LIST_BORDER;
        m_list->setFixedHeight(listH);
        m_popup->setFixedHeight(listH);

        QPoint pos = mapToGlobal(QPoint(0, height()));
        int popupW = popupWidth();
        const QScreen* s = screen();
        if (s != nullptr)
        {
            // Never wider than the screen (rows elide again then), and moved
            // left as far as it takes to keep the right edge on screen.
            const QRect area = s->availableGeometry();
            popupW = std::min(popupW, area.width());
            pos.setX(std::max(area.left(), std::min(pos.x(), area.x() + area.width() - popupW)));
        }
        m_popup->setFixedWidth(popupW);
        m_popup->move(pos);
    }

    // The picker's own width - EXPANDED_PICKER_WIDTH at least, so the popup is
    // readable from a collapsed picker - or wider when that is what it takes
    // to show the longest row's text without eliding.
    [[nodiscard]] int popupWidth() const
    {
        int widest = 0;
        for (int i = 0; i < m_list->count(); ++i)
        {
            QWidget* row = m_list->itemWidget(m_list->item(i));
            if (row == nullptr) continue;
            row->ensurePolished();
            // The row's layout counts its elided labels as zero wide (their
            // size policy is Ignored), so their full text goes on top.
            int textW = 0;
            for (const QLabel* label : row->findChildren<QLabel*>())
            {
                // dynamic_cast: ElideLabel has no Q_OBJECT for qobject_cast.
                const ElideLabel* elided = dynamic_cast<const ElideLabel*>(label);
                if (elided != nullptr)
                {
                    textW += elided->fullTextWidth();
                }
            }
            widest = std::max(widest, row->sizeHint().width() + textW);
        }

        m_popup->ensurePolished();
        int chrome = 2 * (m_popup->frameWidth() + m_list->frameWidth());
        if (m_list->count() > POPUP_MAX_VISIBLE_ROWS)
        {
            chrome += m_list->verticalScrollBar()->sizeHint().width();
        }
        return std::max({width(), EXPANDED_PICKER_WIDTH, widest + chrome});
    }

    void installOnRowWidget(QWidget* w)
    {
        // Skip buttons so they can handle their own click events (e.g. star buttons).
        if (qobject_cast<QAbstractButton*>(w) != nullptr) return;
        w->setMouseTracking(true);
        w->installEventFilter(this);
        for (QObject* child : w->children())
        {
            QWidget* cw = qobject_cast<QWidget*>(child);
            if (cw != nullptr)
            {
                installOnRowWidget(cw);
            }
        }
    }

    // Handles:
    //   • Popup Hide -> reset chevron
    //   • Header click -> togglePopup
    //   • Row Enter/Leave/Click -> hover highlight + item selection callback
    //
    // Subclasses override eventFilter, call this first, and if it returns false
    // they handle any extra cases themselves.
    bool handleCommonEvents(QObject* obj, QEvent* ev)
    {
        // Popup hidden by Qt (outside click auto-dismiss) -> reset chevron
        if (obj == m_popup && ev->type() == QEvent::Hide)
        {
            if (m_chevron != nullptr)
            {
                m_chevron->setText(QStringLiteral("▾"));
            }
            followLayout(false);
            m_rowPressed = false;
            return false;
        }
        if (obj == m_popup && ev->type() == QEvent::Show)
        {
            followLayout(true);
            return false;
        }

        // The picker, or a widget it sits in, moved or changed size while the
        // popup is open (say a favorite added from a row's menu brought the
        // Favorites dropdown in): the popup goes back under the picker.
        if ((ev->type() == QEvent::Move || ev->type() == QEvent::Resize) && m_popup->isVisible() == true
            && obj->isWidgetType() == true
            && (obj == this || static_cast<const QWidget*>(obj)->isAncestorOf(this) == true))
        {
            resizeList();
            return false;
        }

        // Header left-click -> toggle
        if (obj->isWidgetType() && ev->type() == QEvent::MouseButtonRelease
            && static_cast<QMouseEvent*>(ev)->button() == Qt::LeftButton)
        {
            const QWidget* w = qobject_cast<QWidget*>(obj);
            if (w != nullptr && w->objectName() == QLatin1String("locationPickerHeader"))
            {
                const QWidget* p = w;
                while (p != nullptr)
                {
                    if (p == this)
                    {
                        togglePopup();
                        return true;
                    }
                    p = p->parentWidget();
                }
            }
        }

        // Row hover / click
        QWidget* w = qobject_cast<QWidget*>(obj);
        if (w != nullptr)
        {
            QWidget* rowRoot = nullptr;
            QWidget* cur = w;
            while (cur != nullptr)
            {
                if (cur->parent() == m_list->viewport())
                {
                    rowRoot = cur;
                    break;
                }
                cur = qobject_cast<QWidget*>(cur->parent());
            }

            if (rowRoot != nullptr)
            {
                if (ev->type() == QEvent::Enter || ev->type() == QEvent::MouseMove)
                {
                    setHoveredRow(rowRoot);
                    return false;
                }
                if (ev->type() == QEvent::Leave)
                {
                    const QPoint globalPos = QCursor::pos();
                    if (rowRoot->rect().contains(rowRoot->mapFromGlobal(globalPos)) == false &&
                        m_hoveredRow == rowRoot)
                    {
                        setHoveredRow(nullptr);
                    }
                    return false;
                }
                if (ev->type() == QEvent::ContextMenu)
                {
                    const QPoint globalPos = static_cast<QContextMenuEvent*>(ev)->globalPos();
                    const QListWidgetItem* item = m_list->itemAt(m_list->viewport()->mapFromGlobal(globalPos));
                    const RowLocation location = item != nullptr ? rowLocation(item) : RowLocation();
                    if (location.countryCode.isEmpty() == false)
                    {
                        emit rowContextMenuRequested(location.countryCode, location.countryName,
                                                     location.city, globalPos);
                    }
                    return true;
                }
                // A row is picked by a left-click on it, the press and the
                // release: a right-click is for the menu above, and on a row
                // without one does nothing, and a release alone, such as that
                // of a click that closed a row's menu, picks nothing either.
                if (ev->type() == QEvent::MouseButtonPress
                    && static_cast<QMouseEvent*>(ev)->button() == Qt::LeftButton)
                {
                    m_rowPressed = true;
                    return false;
                }
                if (ev->type() == QEvent::MouseButtonRelease
                    && static_cast<QMouseEvent*>(ev)->button() == Qt::LeftButton)
                {
                    const bool pressed = m_rowPressed;
                    m_rowPressed = false;
                    if (pressed == false) return true;
                    const QPoint vp = m_list->viewport()->mapFromGlobal(
                        w->mapToGlobal(static_cast<QMouseEvent*>(ev)->pos()));
                    QListWidgetItem* item = m_list->itemAt(vp);
                    if (item != nullptr)
                    {
                        onRowClicked(item);
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // Subclasses implement this to react to a row click.
    virtual void onRowClicked(QListWidgetItem* item) = 0;

    // The location a row stands for, for its right-click menu: a country
    // code and name, and a city, empty for the country's fastest server. An
    // empty code for a row that is not a location, such as an action.
    struct RowLocation
    {
        QString countryCode;
        QString countryName;
        QString city;
    };
    [[nodiscard]] virtual RowLocation rowLocation(const QListWidgetItem* item) const = 0;

    // Moves the hover highlight to `row` (nullptr clears it).
    //
    // Only the row that is losing the highlight and the one gaining it are
    // restyled - the previous version re-applied a stylesheet to every sibling
    // row on every single mouse-move event, forcing a full style repolish of
    // the whole list each time the pointer moved a pixel.
    //
    // The highlight is a QSS rule keyed on the row's "hovered" property (see
    // style.qss) rather than a local setStyleSheet() on the row: a selector-less
    // local rule also applies to every label and button inside the row, and
    // each of them painted the translucent color again over the row's own,
    // leaving darker boxes behind the text and icons.
    void setHoveredRow(QWidget* row)
    {
        if (m_hoveredRow == row) return;

        if (m_hoveredRow.isNull() == false)
        {
            setStyleProperty(m_hoveredRow, "hovered", false);
        }
        m_hoveredRow = row;
        if (m_hoveredRow.isNull() == false)
        {
            setStyleProperty(m_hoveredRow, "hovered", true);
        }
    }

    ~PickerBase() override
    {
        // m_popup is a parentless top-level (Qt::Popup) window, so it is not
        // destroyed along with this widget the way a child would be.
        delete m_popup;
    }

    void hideEvent(QHideEvent* event) override
    {
        closePopup();
        QFrame::hideEvent(event);
    }

    QFrame*      m_popup   = nullptr;
    QListWidget* m_list    = nullptr;
    QLabel*      m_chevron = nullptr;
    QFrame*      m_header  = nullptr; // set by each subclass after building the header widget

    ElideLabel*  m_topLine    = nullptr;
    ElideLabel*  m_bottomLine = nullptr;
    bool         m_collapsed  = false;

private:
    // While the popup is open, the picker and every widget it sits in report
    // their moves and resizes (see handleCommonEvents()).
    void followLayout(const bool follow)
    {
        for (QWidget* w = this; w != nullptr; w = w->parentWidget())
        {
            if (follow == true)
            {
                w->installEventFilter(this);
            }
            else
            {
                w->removeEventFilter(this);
            }
            if (w->isWindow()) break;
        }
    }

    // A left press on a row since the last release (see handleCommonEvents()).
    bool m_rowPressed = false;
    // QPointer, not a raw pointer: refresh()/populate() call m_list->clear(),
    // which destroys the row widgets - a raw pointer would be left dangling and
    // restyled on the next hover.
    QPointer<QWidget> m_hoveredRow; // row currently painted with the hover style
};

