#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

#include "toggleSwitch.h"

// A ToggleSwitch with a small "ON" / "OFF" label beside it. Drop-in
// replacement for ToggleSwitch, with the same setOn() / isOn() interface and
// the same toggled(bool) signal.
class ToggleWithStatus : public QWidget
{
    static constexpr int TOGGLE_STATUS_SPACING  = 7;
    static constexpr int STATUS_LABEL_MIN_WIDTH = 28;

    Q_OBJECT

public:
    explicit ToggleWithStatus(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(TOGGLE_STATUS_SPACING);

        m_toggle = new ToggleSwitch(this);
        layout->addWidget(m_toggle);

        m_statusLabel = new QLabel(tr("OFF"), this);
        m_statusLabel->setObjectName(QStringLiteral("toggleStatusLabel"));
        m_statusLabel->setMinimumWidth(STATUS_LABEL_MIN_WIDTH);
        layout->addWidget(m_statusLabel);

        connect(m_toggle, &ToggleSwitch::toggled, this, [this](const bool on)
        {
            m_statusLabel->setText(on == true ? tr("ON") : tr("OFF"));
            emit toggled(on);
        });
    }

    [[nodiscard]] bool isOn() const { return m_toggle->isOn(); }

    void setOn(const bool on, const bool animate = true) const
    {
        m_toggle->setOn(on, animate);
        m_statusLabel->setText(on == true ? tr("ON") : tr("OFF"));
    }

    // Sets the state the way a click would, emitting toggled() if it changes.
    void setOnAsUser(const bool on) const
    {
        m_toggle->setOnAsUser(on);
    }

signals:
    void toggled(bool on);

private:
    ToggleSwitch* m_toggle      = nullptr;
    QLabel*       m_statusLabel = nullptr;
};
