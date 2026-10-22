#pragma once

#include <QWidget>

// Animated on/off switch
class ToggleSwitch : public QWidget
{
    static constexpr int TOGGLE_WIDTH  = 44;
    static constexpr int TOGGLE_HEIGHT = 24;

    Q_OBJECT
    Q_PROPERTY(qreal knobPos READ knobPos WRITE setKnobPos)

public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    [[nodiscard]] bool isOn() const { return m_on; }
    // Sets the state without emitting toggled().
    void setOn(const bool on, const bool animate = true);
    // Sets the state the way a click would, emitting toggled() if it changes.
    void setOnAsUser(bool on);

    [[nodiscard]] qreal knobPos() const { return m_knobPos; }

    void setKnobPos(const qreal v)
    {
        m_knobPos = v;
        update();
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        return {TOGGLE_WIDTH, TOGGLE_HEIGHT};
    }

signals:
    void toggled(bool on);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    bool m_on = false;
    qreal m_knobPos = 0.0;
    class QPropertyAnimation* m_anim;
};
