#include "toggleSwitch.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>

namespace
{
constexpr int    ANIM_DURATION_MS     = 150;
constexpr qreal  KNOB_POS_ON          = 1.0;
constexpr int    KNOB_INSET           = 4;
constexpr int    KNOB_OFFSET          = 2;
constexpr QColor TRACK_ON_COLOR       { 0x6d, 0x4a, 0xff };
constexpr QColor TRACK_OFF_COLOR      { 0x55, 0x55, 0x66 };
constexpr QColor TRACK_DISABLED_COLOR { 0x3a, 0x3a, 0x44 };
constexpr QColor KNOB_DISABLED_COLOR  { 0x66, 0x66, 0x77 };
} // namespace

ToggleSwitch::ToggleSwitch(QWidget* parent) : QWidget(parent)
{
    setFixedSize(ToggleSwitch::sizeHint());
    setCursor(Qt::PointingHandCursor);
    m_anim = new QPropertyAnimation(this, "knobPos", this);
    m_anim->setDuration(ANIM_DURATION_MS);
    m_anim->setEasingCurve(QEasingCurve::InOutQuad);
}

void ToggleSwitch::setOn(const bool on, const bool animate)
{
    if (m_on == on) return;
    m_on = on;

    m_anim->stop();

    if (animate == true)
    {
        m_anim->setStartValue(m_knobPos);
        m_anim->setEndValue(on == true ? KNOB_POS_ON : 0.0);
        m_anim->start();
    }
    else
    {
        m_knobPos = on == true ? KNOB_POS_ON : 0.0;
        update();
    }
}

void ToggleSwitch::setOnAsUser(const bool on)
{
    if (m_on == on) return;
    setOn(on);
    emit toggled(m_on);
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool enabled = isEnabled();
    const int  w       = width();
    const int  h       = height();
    const int  r       = h / 2;
    if (enabled == true)
    {
        QColor track;
        track.setRed(static_cast<int>(TRACK_OFF_COLOR.red()   + (TRACK_ON_COLOR.red()   - TRACK_OFF_COLOR.red())   * m_knobPos));
        track.setGreen(static_cast<int>(TRACK_OFF_COLOR.green() + (TRACK_ON_COLOR.green() - TRACK_OFF_COLOR.green()) * m_knobPos));
        track.setBlue(static_cast<int>(TRACK_OFF_COLOR.blue()  + (TRACK_ON_COLOR.blue()  - TRACK_OFF_COLOR.blue())  * m_knobPos));
        p.setBrush(track);
    }
    else
    {
        p.setBrush(TRACK_DISABLED_COLOR);
    }
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(0, 0, w, h, r, r);
    const int knobD   = h - KNOB_INSET;
    const int knobMin = KNOB_OFFSET;
    const int knobMax = w - knobD - KNOB_OFFSET;
    const int knobX   = static_cast<int>(knobMin + (knobMax - knobMin) * m_knobPos);
    p.setBrush(enabled == true ? Qt::white : KNOB_DISABLED_COLOR);
    p.drawEllipse(knobX, KNOB_OFFSET, knobD, knobD);
}

void ToggleSwitch::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
    {
        setOn(m_on == false);
        emit toggled(m_on);
    }
    QWidget::mousePressEvent(e);
}
