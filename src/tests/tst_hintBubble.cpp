#include <QtTest/QtTest>
#include "widgets/hintBubble.h"

// HintBubble::place() decides which side of its target a tip goes on and
// where, so the bubble fits on the page and its arrow points at the target.

namespace
{
using Side = HintBubble::Side;

// A power-button-sized target and a tip's body.
constexpr int TARGET_SIZE = 160;
constexpr int BODY_WIDTH  = 260;
constexpr int BODY_HEIGHT = 120;
const QSize   BODY(BODY_WIDTH, BODY_HEIGHT);
constexpr int ROOMY_WIDTH  = 1000;
constexpr int ROOMY_HEIGHT = 800;
const QRect   ROOMY_BOUNDS(0, 0, ROOMY_WIDTH, ROOMY_HEIGHT);
// Wide enough for the bubble below the target, not beside it.
constexpr int NARROW_WIDTH = 600;
// Too small for the bubble anywhere around the target.
constexpr int TINY_SIZE = 300;
constexpr int TARGET_TOP  = 100;
constexpr int TARGET_LEFT = 200;
// Far enough in for the bubble on any side.
constexpr int CENTER_TOP  = 300;
constexpr int CENTER_LEFT = 400;
// A target this far in leaves no room above or to the left of it.
constexpr int NEAR_EDGE = 10;
} // namespace

class TstHintBubble : public QObject
{
    Q_OBJECT

private slots:
    void place_roomToTheRight_sitsBesideTargetWithArrowOnItsCenter()
    {
        const QRect target(TARGET_LEFT, TARGET_TOP, TARGET_SIZE, TARGET_SIZE);
        const HintBubble::Placement placement =
            HintBubble::place(target, BODY, ROOMY_BOUNDS, {Side::Right, Side::Below});

        QCOMPARE(placement.side, Side::Right);
        QCOMPARE(placement.geometry.x(), target.x() + target.width() + HintBubble::TARGET_GAP);
        QCOMPARE(placement.geometry.size(), QSize(BODY_WIDTH + HintBubble::ARROW_DEPTH, BODY_HEIGHT));
        QCOMPARE(placement.geometry.y() + placement.arrowOffset, target.center().y());
    }

    void place_noRoomBeside_triesTheNextSide()
    {
        const QRect target(TARGET_LEFT, TARGET_TOP, TARGET_SIZE, TARGET_SIZE);
        const QRect bounds(0, 0, NARROW_WIDTH, ROOMY_HEIGHT);
        const HintBubble::Placement placement = HintBubble::place(target, BODY, bounds, {Side::Right, Side::Below});

        QCOMPARE(placement.side, Side::Below);
        QCOMPARE(placement.geometry.y(), target.y() + target.height() + HintBubble::TARGET_GAP);
        QCOMPARE(placement.geometry.size(), QSize(BODY_WIDTH, BODY_HEIGHT + HintBubble::ARROW_DEPTH));
        QCOMPARE(placement.geometry.x() + placement.arrowOffset, target.center().x());
        QVERIFY(bounds.contains(placement.geometry));
    }

    void place_leftAndAbove_endWhereTheTargetStarts()
    {
        const QRect target(CENTER_LEFT, CENTER_TOP, TARGET_SIZE, TARGET_SIZE);

        const HintBubble::Placement left = HintBubble::place(target, BODY, ROOMY_BOUNDS, {Side::Left});
        QCOMPARE(left.side, Side::Left);
        QCOMPARE(left.geometry.x() + left.geometry.width() + HintBubble::TARGET_GAP, target.x());

        const HintBubble::Placement above = HintBubble::place(target, BODY, ROOMY_BOUNDS, {Side::Above});
        QCOMPARE(above.side, Side::Above);
        QCOMPARE(above.geometry.y() + above.geometry.height() + HintBubble::TARGET_GAP, target.y());
    }

    void place_targetNearTheEdge_slidesInsideAndStillPointsAtTarget()
    {
        const QRect target(NEAR_EDGE, TARGET_TOP, TARGET_SIZE, TARGET_SIZE);
        const HintBubble::Placement placement = HintBubble::place(target, BODY, ROOMY_BOUNDS, {Side::Below});

        QCOMPARE(placement.geometry.x(), ROOMY_BOUNDS.x());
        QCOMPARE(placement.geometry.x() + placement.arrowOffset, target.center().x());
    }

    void place_targetInTheCorner_keepsArrowOffTheRoundedCorner()
    {
        const QRect target(0, 0, NEAR_EDGE, NEAR_EDGE);
        const HintBubble::Placement placement = HintBubble::place(target, BODY, ROOMY_BOUNDS, {Side::Right});

        QCOMPARE(placement.geometry.y(), ROOMY_BOUNDS.y());
        QVERIFY(placement.arrowOffset > target.center().y());
        QVERIFY(placement.arrowOffset < placement.geometry.height() / 2);
    }

    void place_fitsNowhere_usesTheLastSide()
    {
        const QRect bounds(0, 0, TINY_SIZE, TINY_SIZE);
        const QRect target(QPoint(0, 0), QSize(TARGET_SIZE, TARGET_SIZE));
        const HintBubble::Placement placement =
            HintBubble::place(target.translated(bounds.center() - target.center()), BODY, bounds,
                              {Side::Right, Side::Below});

        QCOMPARE(placement.side, Side::Below);
    }

    void place_noSides_goesBelow()
    {
        const QRect target(TARGET_LEFT, TARGET_TOP, TARGET_SIZE, TARGET_SIZE);
        QCOMPARE(HintBubble::place(target, BODY, ROOMY_BOUNDS, {}).side, Side::Below);
    }
};

QTEST_MAIN(TstHintBubble)
#include "tst_hintBubble.moc"
