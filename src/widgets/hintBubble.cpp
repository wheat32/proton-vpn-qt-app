#include "hintBubble.h"

#include <algorithm>
#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
// The body's width; its height follows from the text.
constexpr int   BODY_WIDTH         = 260;
constexpr int   BODY_H_PADDING     = 14;
constexpr int   BODY_V_PADDING     = 12;
constexpr int   BODY_SPACING       = 6;
constexpr int   CORNER_RADIUS      = 10;
constexpr int   ARROW_HALF_WIDTH   = 9;
// The arrow's base lies this far inside the body, so the two join without
// a seam.
constexpr int   ARROW_BASE_OVERLAP = 1;
// The least room kept between the bubble and the edge of its bounds.
constexpr int   EDGE_MARGIN        = 8;
// The arrow stays clear of the body's rounded corners.
constexpr int   ARROW_MIN_OFFSET   = CORNER_RADIUS + ARROW_HALF_WIDTH;
constexpr QColor BUBBLE_COLOR(0x6d, 0x4a, 0xff);

// `value` moved as little as it takes to lie within [low, high]; low when the
// range is empty, so a bubble too big for its bounds starts at their edge.
int clampTo(const int value, const int low, const int high)
{
    return std::max(low, std::min(value, high));
}

HintBubble::Placement placeOn(const HintBubble::Side side, const QRect& target, const QSize& bodySize,
                              const QRect& bounds)
{
    using Side = HintBubble::Side;

    const QPoint center = target.center();
    // The arrow adds to the bubble's size on the side facing the target.
    const bool beside = side == Side::Right || side == Side::Left;
    const QSize size = beside == true
        ? QSize(bodySize.width() + HintBubble::ARROW_DEPTH, bodySize.height())
        : QSize(bodySize.width(), bodySize.height() + HintBubble::ARROW_DEPTH);

    QPoint topLeft;
    switch (side)
    {
        case Side::Right:
            topLeft = QPoint(target.x() + target.width() + HintBubble::TARGET_GAP,
                             center.y() - size.height() / 2);
            break;

        case Side::Left:
            topLeft = QPoint(target.x() - HintBubble::TARGET_GAP - size.width(),
                             center.y() - size.height() / 2);
            break;

        case Side::Below:
            topLeft = QPoint(center.x() - size.width() / 2,
                             target.y() + target.height() + HintBubble::TARGET_GAP);
            break;

        case Side::Above:
            topLeft = QPoint(center.x() - size.width() / 2,
                             target.y() - HintBubble::TARGET_GAP - size.height());
            break;
    }

    int towardCenter = 0;
    int edgeLength = 0;
    if (beside == true)
    {
        topLeft.setY(clampTo(topLeft.y(), bounds.y(), bounds.y() + bounds.height() - size.height()));
        towardCenter = center.y() - topLeft.y();
        edgeLength = size.height();
    }
    else
    {
        topLeft.setX(clampTo(topLeft.x(), bounds.x(), bounds.x() + bounds.width() - size.width()));
        towardCenter = center.x() - topLeft.x();
        edgeLength = size.width();
    }

    return {.side = side,
            .geometry = QRect(topLeft, size),
            .arrowOffset = clampTo(towardCenter, ARROW_MIN_OFFSET, edgeLength - ARROW_MIN_OFFSET)};
}
} // namespace

HintBubble::HintBubble(const QString& title, const QString& text, QWidget* target, QWidget* parent)
    : QWidget(parent), m_target(target)
{
    // Shown by reposition(), once it knows where.
    hide();
    setAccessibleName(title);
    setAccessibleDescription(text);

    // The app's stylesheet gives every plain QWidget the page background,
    // which would cover the bubble drawn behind the body.
    setStyleSheet(QStringLiteral(
        "QWidget#hintBubbleBody { background: transparent; }"
        "QLabel { color: white; background: transparent; }"
        "QLabel#hintBubbleTitle { font-weight: bold; font-size: 14px; }"
        "QPushButton#hintBubbleButton { background-color: white; color: %1; font-weight: bold;"
        " border: 2px solid white; border-radius: 6px; padding: 3px 14px; }"
        "QPushButton#hintBubbleButton:hover { background-color: #e6e0ff; border-color: #e6e0ff; }"
        "QPushButton#hintBubbleButton:focus { border-color: #1a1a2e; }")
        .arg(BUBBLE_COLOR.name()));

    m_body = new QWidget(this);
    m_body->setObjectName(QStringLiteral("hintBubbleBody"));
    QVBoxLayout* layout = new QVBoxLayout(m_body);
    layout->setContentsMargins(BODY_H_PADDING, BODY_V_PADDING, BODY_H_PADDING, BODY_V_PADDING);
    layout->setSpacing(BODY_SPACING);

    QLabel* titleLabel = new QLabel(title, m_body);
    titleLabel->setObjectName(QStringLiteral("hintBubbleTitle"));
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    QLabel* textLabel = new QLabel(text, m_body);
    textLabel->setWordWrap(true);
    layout->addWidget(textLabel);

    QPushButton* button = new QPushButton(tr("Got It!"), m_body);
    button->setObjectName(QStringLiteral("hintBubbleButton"));
    button->setCursor(Qt::PointingHandCursor);
    connect(button, &QPushButton::clicked, this, &HintBubble::dismiss);
    layout->addWidget(button, 0, Qt::AlignRight);

    watchLayout();
    scheduleReposition();
}

void HintBubble::setSides(const QList<Side>& sides)
{
    m_sides = sides;
    scheduleReposition();
}

void HintBubble::setBounds(QWidget* bounds)
{
    m_bounds = bounds;
    watchLayout();
    scheduleReposition();
}

// static
HintBubble::Placement HintBubble::place(const QRect& target, const QSize& bodySize,
                                        const QRect& bounds, const QList<Side>& sides)
{
    Placement placement = placeOn(Side::Below, target, bodySize, bounds);
    for (const Side side : sides)
    {
        placement = placeOn(side, target, bodySize, bounds);
        if (bounds.contains(placement.geometry) == true) break;
    }
    return placement;
}

void HintBubble::dismiss()
{
    if (m_dismissed == true) return;
    m_dismissed = true;
    hide();
    emit dismissed();
    deleteLater();
}

bool HintBubble::eventFilter(QObject* obj, QEvent* ev)
{
    switch (ev->type())
    {
        case QEvent::ParentChange:
            // Watched again from reposition(): changing event filters while
            // one is running is not safe.
            m_rewatchPending = true;
            scheduleReposition();
            break;

        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::Hide:
            scheduleReposition();
            break;

        default:
            break;
    }
    return QWidget::eventFilter(obj, ev);
}

void HintBubble::watchLayout()
{
    for (const QPointer<QWidget>& widget : std::as_const(m_watched))
    {
        if (widget != nullptr)
        {
            widget->removeEventFilter(this);
        }
    }
    m_watched.clear();

    QList<QWidget*> widgets{parentWidget(), m_bounds};
    for (QWidget* widget = m_target; widget != nullptr && widget != parentWidget(); widget = widget->parentWidget())
    {
        widgets.append(widget);
    }
    for (QWidget* widget : std::as_const(widgets))
    {
        if (widget == nullptr || m_watched.contains(widget) == true) continue;
        widget->installEventFilter(this);
        m_watched.append(widget);
    }
}

void HintBubble::scheduleReposition()
{
    // Deferred so the target's position comes from the finished layout pass,
    // and coalesced so a burst of moves costs one reposition.
    if (m_repositionPending == true) return;
    m_repositionPending = true;
    QTimer::singleShot(0, this, [this]()
    {
        m_repositionPending = false;
        reposition();
    });
}

void HintBubble::reposition()
{
    if (m_dismissed == true) return;
    if (m_rewatchPending == true)
    {
        m_rewatchPending = false;
        watchLayout();
    }

    // Hidden with the target, or if the target has left the parent.
    QWidget* parent = parentWidget();
    if (parent == nullptr || m_target == nullptr || parent->isAncestorOf(m_target) == false
        || m_target->isVisibleTo(parent) == false)
    {
        hide();
        return;
    }

    const QRect target(m_target->mapTo(parent, QPoint(0, 0)), m_target->size());
    QRect bounds = parent->rect();
    if (m_bounds != nullptr && parent->isAncestorOf(m_bounds) == true)
    {
        bounds = QRect(m_bounds->mapTo(parent, QPoint(0, 0)), m_bounds->size());
    }
    bounds = bounds.marginsRemoved(QMargins(EDGE_MARGIN, EDGE_MARGIN, EDGE_MARGIN, EDGE_MARGIN));

    const int bodyHeight = m_body->hasHeightForWidth() == true
        ? m_body->heightForWidth(BODY_WIDTH)
        : m_body->sizeHint().height();
    m_placement = place(target, QSize(BODY_WIDTH, bodyHeight), bounds, m_sides);
    setGeometry(m_placement.geometry);
    m_body->setGeometry(bodyRect());
    show();
    raise();
    update();
}

QRect HintBubble::bodyRect() const
{
    switch (m_placement.side)
    {
        case Side::Right:
            return rect().adjusted(ARROW_DEPTH, 0, 0, 0);

        case Side::Left:
            return rect().adjusted(0, 0, -ARROW_DEPTH, 0);

        case Side::Below:
            return rect().adjusted(0, ARROW_DEPTH, 0, 0);

        case Side::Above:
            return rect().adjusted(0, 0, 0, -ARROW_DEPTH);
    }
    return rect();
}

void HintBubble::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath bubble;
    bubble.addRoundedRect(QRectF(bodyRect()), CORNER_RADIUS, CORNER_RADIUS);

    // The arrow's tip at the bubble's edge, then the two ends of its base.
    const qreal along = m_placement.arrowOffset;
    const qreal baseIn = ARROW_DEPTH + ARROW_BASE_OVERLAP;
    QPolygonF arrow;
    switch (m_placement.side)
    {
        case Side::Right:
            arrow << QPointF(0, along)
                  << QPointF(baseIn, along - ARROW_HALF_WIDTH) << QPointF(baseIn, along + ARROW_HALF_WIDTH);
            break;

        case Side::Left:
            arrow << QPointF(width(), along)
                  << QPointF(width() - baseIn, along - ARROW_HALF_WIDTH)
                  << QPointF(width() - baseIn, along + ARROW_HALF_WIDTH);
            break;

        case Side::Below:
            arrow << QPointF(along, 0)
                  << QPointF(along - ARROW_HALF_WIDTH, baseIn) << QPointF(along + ARROW_HALF_WIDTH, baseIn);
            break;

        case Side::Above:
            arrow << QPointF(along, height())
                  << QPointF(along - ARROW_HALF_WIDTH, height() - baseIn)
                  << QPointF(along + ARROW_HALF_WIDTH, height() - baseIn);
            break;
    }
    QPainterPath arrowPath;
    arrowPath.addPolygon(arrow);
    arrowPath.closeSubpath();

    painter.fillPath(bubble.united(arrowPath), BUBBLE_COLOR);
}
