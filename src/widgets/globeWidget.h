#pragma once

// globeWidget.h
// The animated globe behind the VPN page's content. It spins slowly on a
// tilted axis like a desk globe and, while a connection is being made or is
// up, turns to face that country dead-on. Purely decorative: it takes no
// mouse input and renders nothing while hidden.

#include <QElapsedTimer>
#include <QPixmap>
#include <QPointer>
#include <QQuaternion>
#include <QTimer>
#include <QWidget>
#include <optional>
#include <tuple>
#include "../globe/globeMath.h"
#include "../globe/globeRenderer.h"
#include "pixmapCache.h"

class GlobeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GlobeWidget(QWidget* parent = nullptr);

    // The globe is centered horizontally on this widget's center (the power
    // button) and vertically on the globe widget itself. It
    // must be a descendant of this widget's parent.
    void setAnchor(QWidget* anchor);

    // Longitude facing the viewer while spinning, before any target is set.
    void setSpinLongitude(double longitude);

    // Turns to face `target`, or back to the idle spin for nullopt.
    void setTarget(const std::optional<GeoPoint>& target);

    // Repaints after the anchor moved without this widget being resized.
    void anchorMoved();

    // Stops the idle spin while the window is not the active one. A turn
    // toward or away from a country still finishes, so the globe always ends
    // up facing the right way.
    void setPauseWhenInactive(bool pause);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    enum class Motion
    {
        Spinning, // idle: slow rotation about the tilted axis
        Turning,  // easing from one orientation to another
        Holding,  // facing the target; static, nothing to animate
    };

    // An orientation that is not mid-turn, split the way the renderer's fast
    // path takes it: a fixed frame, then a spin to `longitude` (see GlobeMath).
    struct Pose
    {
        QQuaternion frame;
        double longitude;
    };

    void tick();
    void finishTurn();
    void updateTimer();
    void updateColors();
    [[nodiscard]] QQuaternion turnOrientation() const;
    [[nodiscard]] Pose restingPose() const;
    [[nodiscard]] QQuaternion currentOrientation() const;
    // The disc's size in logical pixels before rounding (see renderSize()).
    [[nodiscard]] qreal wantedDiameter() const;
    [[nodiscard]] int renderSize() const;
    [[nodiscard]] QRectF discRect() const;
    // The area the disc and its glow cover.
    [[nodiscard]] static QRectF withGlow(const QRectF& disc);
    [[nodiscard]] QRect paintRect() const;

    GlobeRenderer m_renderer;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastTickMs = 0;

    Motion m_motion = Motion::Spinning;
    double m_spinLongitude = 0.0;
    std::optional<GeoPoint> m_target;
    QQuaternion m_turnFrom;
    QQuaternion m_turnTo;
    qint64 m_turnStartMs    = 0;
    qint64 m_turnDurationMs = 0;

    QPointer<QWidget> m_anchor;
    QColor m_glowColor;
    bool m_pauseWhenInactive = false;

    // The renderer's current image is reused until something that changes it
    // (orientation, size, colors) clears this. Holding on a country then
    // renders once, however often the content above it repaints.
    bool m_frameValid = false;
    // Where the disc was last drawn, to catch it moving under a partial repaint.
    QRectF m_paintedDisc;
    // The glow by size, screen ratio and color.
    PixmapCache<std::tuple<QSizeF, qreal, QColor>> m_glowCache;
};
