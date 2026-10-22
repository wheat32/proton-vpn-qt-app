#pragma once

#include <QList>
#include <QPointer>
#include <QRect>
#include <QWidget>

// ---------------------------------------------------------------------------
// HintBubble – a callout that points at a widget to explain it, like a
// first-launch tip. It sits on a side of the target that has room for it,
// follows the target as the page lays out again, and stays until its button
// (or dismiss()) closes it. It deletes itself once dismissed.
// ---------------------------------------------------------------------------
class HintBubble : public QWidget
{
    Q_OBJECT

public:
    // The side of the target the bubble sits on; its arrow points back at it.
    enum class Side { Right, Below, Left, Above };

    struct Placement
    {
        Side  side = Side::Below;
        // The whole bubble, arrow included, in the bounds' coordinates.
        QRect geometry;
        // Where the arrow leaves the bubble, along the edge facing the
        // target: from its top for Right and Left, its left end for Below
        // and Above.
        int   arrowOffset = 0;
    };

    // The space between the target and the arrow's tip.
    static constexpr int TARGET_GAP  = 6;
    // How far the arrow reaches out of the bubble.
    static constexpr int ARROW_DEPTH = 9;

    // Drawn over `parent`, pointing at `target`, which must be inside it.
    HintBubble(const QString& title, const QString& text, QWidget* target, QWidget* parent);

    // The sides to try, in order (see place()). Below, Above, Right, Left
    // by default.
    void setSides(const QList<Side>& sides);
    // Keeps the bubble inside `bounds`, a widget inside the parent, rather
    // than inside the whole parent.
    void setBounds(QWidget* bounds);

    // Where a bubble whose body (the bubble less its arrow) is `bodySize`
    // goes beside `target`: on the first of `sides` where it fits within
    // `bounds`, or on the last when it fits on none. It slides along that
    // side of the target to stay within the bounds, the arrow still
    // pointing at the target's center.
    [[nodiscard]] static Placement place(const QRect& target, const QSize& bodySize,
                                         const QRect& bounds, const QList<Side>& sides);

signals:
    void dismissed();

public slots:
    void dismiss();

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void paintEvent(QPaintEvent* event) override;

private:
    // Watches the target, each widget between it and the parent, the parent
    // and the bounds: a move or resize of any of them can move the bubble.
    void watchLayout();
    // Runs reposition() once the pending layout pass is done.
    void scheduleReposition();
    void reposition();
    // The bubble less its arrow.
    [[nodiscard]] QRect bodyRect() const;

    QPointer<QWidget> m_target;
    QPointer<QWidget> m_bounds;
    QList<QPointer<QWidget>> m_watched;
    QList<Side> m_sides{Side::Below, Side::Above, Side::Right, Side::Left};
    QWidget*  m_body = nullptr;
    Placement m_placement;
    bool m_repositionPending = false;
    // Set when a watched widget moved to another parent, so the widgets
    // between the target and the parent are watched afresh.
    bool m_rewatchPending    = false;
    bool m_dismissed         = false;
};
