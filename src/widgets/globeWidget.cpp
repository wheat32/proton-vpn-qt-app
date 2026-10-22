#include "globeWidget.h"
#include "../themeManager.h"

#include <QEasingCurve>
#include <QEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QRegion>
#include <algorithm>
#include <cmath>

namespace
{
// One revolution every two minutes. The spin is slow enough that 15 frames a
// second still moves the surface less than a pixel per frame at the default
// window size, and every frame saved matters for an app left open all day.
constexpr double SPIN_DEGREES_PER_SECOND = 3.0;
constexpr int    SPIN_FRAME_MS           = 66;
constexpr int    TURN_FRAME_MS           = 20;

// A turn takes TURN_MIN_MS plus up to TURN_MS_PER_HALF_TURN more for the
// largest possible swing, so short hops are quick and long ones unhurried.
constexpr qint64 TURN_MIN_MS           = 900;
constexpr qint64 TURN_MS_PER_HALF_TURN = 1300;
// Below this, a requested turn is skipped and the end state applied directly.
constexpr double TURN_SKIP_DEGREES     = 0.5;

// After a pause (hidden page, minimized window) the spin resumes from where
// it was rather than jumping ahead by the time it was not visible.
constexpr qint64 MAX_TICK_GAP_MS = 200;

constexpr double MS_PER_SECOND         = 1000.0;

// Size of the globe relative to the page's shorter side, and the smallest it
// gets: below that a small window cuts it off at the edges instead.
constexpr qreal DIAMETER_FACTOR = 0.92;
constexpr qreal MIN_DIAMETER    = 600.0;

// Render resolution: the disc's size in device pixels, rounded to a step so a
// window drag does not rebuild the renderer's tables on every pixel, and
// capped so a full-screen window does not cost more than it shows.
constexpr int RENDER_SIZE_STEP = 16;
constexpr int MIN_RENDER_SIZE  = 64;
constexpr int MAX_RENDER_SIZE  = 640;

// Soft glow around the disc, as a fraction of its radius.
constexpr qreal GLOW_WIDTH = 0.06;

// Dark theme: deep purple water, lighter purple land.
constexpr QColor DARK_WATER(0x2a, 0x22, 0x5c, 0xc0);
constexpr QColor DARK_LAND(0x6c, 0x58, 0xdc, 0xc8);
constexpr QColor DARK_GLOW(0x6d, 0x4a, 0xff, 0x38);
// Light theme: the same idea in pale tones, water still the darker of the two.
constexpr QColor LIGHT_WATER(0xc9, 0xbe, 0xf7, 0xe0);
constexpr QColor LIGHT_LAND(0xf1, 0xed, 0xff, 0xe8);
constexpr QColor LIGHT_GLOW(0x6d, 0x4a, 0xff, 0x28);

// Shading (ambient, diffuse): the renderer's default, strong for a lit-sphere
// look, on the dark page; gentle on the light page so pale colors stay pale.
constexpr float LIGHT_AMBIENT  = 0.90f;
constexpr float LIGHT_DIFFUSE  = 0.10f;
} // namespace

GlobeWidget::GlobeWidget(QWidget* parent) : QWidget(parent)
{
    // Decoration only: clicks belong to whatever is underneath.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);

    m_renderer.setLandMask(QImage(QStringLiteral(":/globe/land.png")));
    updateColors();

    m_clock.start();
    connect(&m_timer, &QTimer::timeout, this, &GlobeWidget::tick);
}

void GlobeWidget::setAnchor(QWidget* anchor)
{
    m_anchor = anchor;
    update();
}

void GlobeWidget::setSpinLongitude(const double longitude)
{
    m_spinLongitude = longitude;
    m_frameValid = false;
    update();
}

void GlobeWidget::anchorMoved()
{
    update();
}

void GlobeWidget::setPauseWhenInactive(const bool pause)
{
    m_pauseWhenInactive = pause;
    updateTimer();
}

void GlobeWidget::setTarget(const std::optional<GeoPoint>& target)
{
    if (target == m_target) return;

    // Read before m_target changes: while holding, the orientation is the
    // old target's.
    const QQuaternion from = currentOrientation();
    m_target = target;

    // Back to idle without spinning backwards: the tilt returns around
    // whichever longitude is facing the viewer now.
    const QQuaternion to = target.has_value()
        ? GlobeMath::lookAt(*target)
        : GlobeMath::idle(GlobeMath::centerOf(from).longitude);

    m_turnFrom = from;
    m_turnTo   = to;
    m_frameValid = false;
    const double angle = GlobeMath::angleBetween(from, to);
    if (angle < TURN_SKIP_DEGREES)
    {
        finishTurn();
        return;
    }
    m_turnStartMs    = m_clock.elapsed();
    m_turnDurationMs = TURN_MIN_MS + static_cast<qint64>(angle / GlobeMath::DEGREES_PER_HALF_TURN * TURN_MS_PER_HALF_TURN);
    m_motion = Motion::Turning;
    updateTimer();
    update(paintRect());
}

void GlobeWidget::finishTurn()
{
    if (m_target.has_value() == true)
    {
        m_motion = Motion::Holding;
    }
    else
    {
        m_motion = Motion::Spinning;
        m_spinLongitude = GlobeMath::centerOf(m_turnTo).longitude;
    }
    m_frameValid = false;
    updateTimer();
    update(paintRect());
}

QQuaternion GlobeWidget::turnOrientation() const
{
    const qreal progress = (m_turnDurationMs > 0)
        ? std::clamp(static_cast<qreal>(m_clock.elapsed() - m_turnStartMs) / m_turnDurationMs, 0.0, 1.0)
        : 1.0;
    const QEasingCurve easing(QEasingCurve::InOutCubic);
    return QQuaternion::slerp(m_turnFrom, m_turnTo, static_cast<float>(easing.valueForProgress(progress)));
}

GlobeWidget::Pose GlobeWidget::restingPose() const
{
    if (m_motion == Motion::Holding && m_target.has_value() == true)
    {
        return {GlobeMath::lookAtFrame(m_target->latitude), m_target->longitude};
    }
    return {GlobeMath::idleFrame(), m_spinLongitude};
}

QQuaternion GlobeWidget::currentOrientation() const
{
    if (m_motion == Motion::Turning)
    {
        return turnOrientation();
    }
    const Pose pose = restingPose();
    return pose.frame * GlobeMath::spin(pose.longitude);
}

void GlobeWidget::tick()
{
    const qint64 now = m_clock.elapsed();
    const qint64 elapsed = std::min(now - m_lastTickMs, MAX_TICK_GAP_MS);
    m_lastTickMs = now;

    if (m_motion == Motion::Spinning)
    {
        // The surface moves west to east, as Earth's does, so the longitude
        // facing the viewer decreases.
        m_spinLongitude = std::fmod(m_spinLongitude - SPIN_DEGREES_PER_SECOND * elapsed / MS_PER_SECOND,
                                    GlobeMath::DEGREES_PER_TURN);
    }
    else if (m_motion == Motion::Turning && now - m_turnStartMs >= m_turnDurationMs)
    {
        finishTurn();
        return;
    }
    m_frameValid = false;
    update(paintRect());
}

void GlobeWidget::updateTimer()
{
    const bool paused = m_motion == Motion::Spinning && m_pauseWhenInactive == true
                     && isActiveWindow() == false;
    const bool animating = isVisible() == true && m_motion != Motion::Holding && paused == false;
    if (animating == false)
    {
        m_timer.stop();
        return;
    }
    const int interval = (m_motion == Motion::Turning) ? TURN_FRAME_MS : SPIN_FRAME_MS;
    if (m_timer.isActive() == false)
    {
        m_lastTickMs = m_clock.elapsed();
    }
    if (m_timer.isActive() == false || m_timer.interval() != interval)
    {
        m_timer.start(interval);
    }
}

void GlobeWidget::updateColors()
{
    const bool dark = ThemeManager::isDark();
    m_renderer.setColors(dark ? DARK_WATER : LIGHT_WATER, dark ? DARK_LAND : LIGHT_LAND);
    m_renderer.setLighting(dark ? GlobeRenderer::DEFAULT_AMBIENT : LIGHT_AMBIENT,
                           dark ? GlobeRenderer::DEFAULT_DIFFUSE : LIGHT_DIFFUSE);
    m_glowColor = dark ? DARK_GLOW : LIGHT_GLOW;
    m_frameValid = false;
}

qreal GlobeWidget::wantedDiameter() const
{
    return std::max(std::min(width(), height()) * DIAMETER_FACTOR, MIN_DIAMETER);
}

int GlobeWidget::renderSize() const
{
    const qreal wanted = wantedDiameter() * devicePixelRatioF();
    return std::clamp(static_cast<int>(std::lround(wanted / RENDER_SIZE_STEP)) * RENDER_SIZE_STEP,
                      MIN_RENDER_SIZE, MAX_RENDER_SIZE);
}

QRectF GlobeWidget::discRect() const
{
    const qreal dpr = devicePixelRatioF();
    const int size = renderSize();
    // Shown at exactly the rendered size, on whole device pixels, so drawing
    // it is a plain copy. Only when the render size is capped (a very large
    // window, or a high-DPI screen) is it scaled up instead.
    const qreal diameter = (size < MAX_RENDER_SIZE)
        ? size / dpr
        : wantedDiameter();
    QPointF center = QRectF(rect()).center();
    if (m_anchor.isNull() == false && parentWidget() != nullptr)
    {
        // Through the shared parent: mapTo() only maps to an ancestor.
        center.setX(mapFromParent(m_anchor->mapTo(parentWidget(), QRectF(m_anchor->rect()).center())).x());
    }
    const qreal left = std::round((center.x() - diameter / 2.0) * dpr) / dpr;
    const qreal top  = std::round((center.y() - diameter / 2.0) * dpr) / dpr;
    return {left, top, diameter, diameter};
}

// static
QRectF GlobeWidget::withGlow(const QRectF& disc)
{
    const qreal glow = disc.width() / 2.0 * GLOW_WIDTH;
    return disc.adjusted(-glow, -glow, glow, glow);
}

QRect GlobeWidget::paintRect() const
{
    return withGlow(discRect()).toAlignedRect();
}

void GlobeWidget::paintEvent(QPaintEvent* event)
{
    const QRectF disc = discRect();
    if (disc.width() <= 0.0) return;

    // The disc follows the power button, which a layout change can move after
    // this widget was last painted. Drawing only the requested part at the new
    // position would leave the rest of the old disc on screen, so if the
    // request does not cover both, redraw the whole of both next.
    if (disc != m_paintedDisc)
    {
        // Each disc with its own glow: after a resize the old one's is wider
        // or narrower than the new one's.
        const QRect both = withGlow(disc).united(withGlow(m_paintedDisc)).toAlignedRect();
        // Not QRegion::contains(), which is true when the region merely
        // overlaps the rectangle: what matters is whether any of it is left.
        if (QRegion(both).subtracted(event->region()).isEmpty() == false)
        {
            update(both);
        }
        m_paintedDisc = disc;
    }

    const int size = renderSize();
    if (size != m_renderer.size())
    {
        m_renderer.setSize(size);
        m_frameValid = false;
    }

    if (m_frameValid == false)
    {
        if (m_motion == Motion::Turning)
        {
            (void)m_renderer.render(turnOrientation());
        }
        else
        {
            // The renderer's fast path: only the longitude moves between frames.
            const Pose pose = restingPose();
            (void)m_renderer.renderSpin(pose.frame, pose.longitude);
        }
        m_frameValid = true;
    }

    // Glow first, so the globe covers its inner half. Drawn once per size and
    // color into a pixmap: a radial gradient is costly to fill every frame.
    const qreal dpr = devicePixelRatioF();
    const qreal radius = disc.width() / 2.0;
    const QSizeF glowSize = withGlow(disc).size();
    const qreal glowRadius = glowSize.width() / 2.0;
    const QPixmap& glowPixmap = m_glowCache.get({glowSize, dpr, m_glowColor}, [&]()
    {
        QPixmap pixmap = blankPixmap(glowSize, dpr);
        QPainter gp(&pixmap);
        gp.setRenderHint(QPainter::Antialiasing);
        QRadialGradient glow(QPointF(glowRadius, glowRadius), glowRadius);
        glow.setColorAt(radius / glowRadius, m_glowColor);
        glow.setColorAt(1.0, Qt::transparent);
        gp.setPen(Qt::NoPen);
        gp.setBrush(glow);
        gp.drawEllipse(QPointF(glowRadius, glowRadius), glowRadius, glowRadius);
        gp.end();
        return pixmap;
    });

    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawPixmap(disc.center() - QPointF(glowRadius, glowRadius), glowPixmap);
    p.drawImage(disc, m_renderer.image());
}

void GlobeWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updateTimer();
}

void GlobeWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    updateTimer();
}

bool GlobeWidget::event(QEvent* event)
{
    // A child widget learns of its window gaining or losing focus through
    // these, which QWidget::event passes down from the window; ActivationChange
    // only reaches the window itself.
    if (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate)
    {
        updateTimer();
    }
    return QWidget::event(event);
}

void GlobeWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        updateColors();
        update();
    }
}
