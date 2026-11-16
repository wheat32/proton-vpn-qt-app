#pragma once

#include <QEvent>
#include <QFile>
#include <QFocusEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSvgRenderer>
#include <QVBoxLayout>

// This is a styled replacement for QSpinBox that
//  renders its up/down chevrons from SVG resources correctly,
//  even when the SVGs use fill="currentColor".

class NumberSpinner : public QFrame
{
    Q_OBJECT

    static constexpr int SPINNER_WIDTH   = 64;
    static constexpr int SPINNER_HEIGHT  = 38;
    static constexpr int ARROW_BTN_WIDTH = 24;
    // The two arrow buttons share the spinner's height.
    static constexpr int ARROW_BTN_COUNT  = 2;
    static constexpr int ARROW_BTN_HEIGHT = SPINNER_HEIGHT / ARROW_BTN_COUNT;
    static constexpr int ARROW_ICON_SIZE  = 10;
    // Left of the buttons, where their frame's border shows as a separator.
    static constexpr int SEPARATOR_WIDTH  = 1;
    static constexpr int DEFAULT_MAXIMUM  = 99;
    static constexpr int STEP             = 1;

public:
    explicit NumberSpinner(QWidget* parent = nullptr) : QFrame(parent)
    {
        setObjectName(QStringLiteral("numberSpinner"));
        setFixedWidth(SPINNER_WIDTH);
        setFixedHeight(SPINNER_HEIGHT);

        QHBoxLayout* outer = new QHBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        m_display = new QLineEdit(this);
        m_display->setAlignment(Qt::AlignCenter);
        m_display->setObjectName(QStringLiteral("numberSpinnerDisplay"));
        m_validator = new QIntValidator(m_min, m_max, m_display);
        m_display->setValidator(m_validator);
        // The validator only keeps letters out. It lets a number outside the
        // range, or no number at all, stand while it is being typed, and
        // QLineEdit then never reports the editing as finished; so the end of
        // the editing is taken from the field's own events (see eventFilter()).
        m_display->installEventFilter(this);
        outer->addWidget(m_display, 1);

        QFrame* btnFrame = new QFrame(this);
        btnFrame->setObjectName(QStringLiteral("numberSpinnerButtons"));
        QVBoxLayout* btnLayout = new QVBoxLayout(btnFrame);
        btnLayout->setContentsMargins(SEPARATOR_WIDTH, 0, 0, 0);
        btnLayout->setSpacing(0);

        m_upBtn   = makeArrowBtn(QStringLiteral(":/assets/chevron-up.svg"));
        m_downBtn = makeArrowBtn(QStringLiteral(":/assets/chevron-down.svg"));
        // Distinct names so QSS can round the correct corners of each button.
        m_upBtn->setObjectName(QStringLiteral("numberSpinnerBtnUp"));
        m_downBtn->setObjectName(QStringLiteral("numberSpinnerBtnDown"));
        btnLayout->addWidget(m_upBtn);
        btnLayout->addWidget(m_downBtn);
        outer->addWidget(btnFrame);

        connect(m_upBtn, &QPushButton::clicked, this, [this]()
        {
            step(STEP);
        });
        connect(m_downBtn, &QPushButton::clicked, this, [this]()
        {
            step(-STEP);
        });

        refreshDisplay();
    }

    void setRange(const int min, const int max)
    {
        m_min = min;
        m_max = max;
        m_value = qBound(m_min, m_value, m_max);
        m_validator->setRange(m_min, m_max);

        refreshDisplay();
        refreshButtons();
    }

    void setValue(const int value)
    {
        m_value = qBound(m_min, value, m_max);
        refreshDisplay();
        refreshButtons();
    }

    [[nodiscard]] int value() const { return m_value; }
    [[nodiscard]] int minimum() const { return m_min; }
    [[nodiscard]] int maximum() const { return m_max; }

    // Sets the value the way the arrows or typing would, emitting
    // valueChanged() if it changes. setValue() emits nothing.
    void setValueAsUser(const int value)
    {
        const int clamped = qBound(m_min, value, m_max);
        if (clamped == m_value) return;
        setValue(clamped);
        emit valueChanged(m_value);
    }

    // The value once the user is done typing `text`: the number typed when it
    // is one within the range, and `current` otherwise (nothing typed, not a
    // number, or a number outside the range).
    [[nodiscard]] static int valueFromText(const QString& text, const int min, const int max, const int current)
    {
        bool ok = false;
        const int typed = text.trimmed().toInt(&ok);
        if (ok == false || typed < min || typed > max)
        {
            return current;
        }
        return typed;
    }

signals:
    void valueChanged(int value);

protected:
    void changeEvent(QEvent* e) override
    {
        QFrame::changeEvent(e);
        if (e->type() == QEvent::PaletteChange)
        {
            refreshArrowIcons();
        }
    }

    // The user is done typing when Return is pressed or the field loses the
    // focus (not to its own context menu, which leaves the typing as it is).
    bool eventFilter(QObject* obj, QEvent* e) override
    {
        if (obj == m_display)
        {
            bool done = false;
            if (e->type() == QEvent::KeyPress)
            {
                const int key = static_cast<const QKeyEvent*>(e)->key();
                done = key == Qt::Key_Return || key == Qt::Key_Enter;
            }
            else if (e->type() == QEvent::FocusOut)
            {
                done = static_cast<const QFocusEvent*>(e)->reason() != Qt::PopupFocusReason;
            }
            if (done == true)
            {
                commitText();
            }
        }
        return QFrame::eventFilter(obj, e);
    }

private:
    // Load an SVG from a Qt resource, patch currentColor to the current palette
    // text color, and return a QIcon.  Called at construction and on palette change.
    static QIcon svgIcon(const QString& resource)
    {
        QFile file(resource);
        if (file.open(QIODevice::ReadOnly) == false)
        {
            return {};
        }

        QByteArray data = file.readAll();
        const QColor textColor = QGuiApplication::palette().color(QPalette::WindowText);
        data.replace("currentColor", textColor.name().toLatin1());

        QSvgRenderer renderer(data);
        QPixmap pixmap(ARROW_ICON_SIZE, ARROW_ICON_SIZE);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        return QIcon(pixmap);
    }

    QPushButton* makeArrowBtn(const QString& resource)
    {
        QPushButton* btn = new QPushButton(this);
        btn->setIcon(svgIcon(resource));
        btn->setIconSize({ARROW_ICON_SIZE, ARROW_ICON_SIZE});
        btn->setFixedWidth(ARROW_BTN_WIDTH);
        btn->setFixedHeight(ARROW_BTN_HEIGHT);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        return btn;
    }

    void step(const int delta)
    {
        const int next = m_value + delta;
        if (next < m_min || next > m_max) return;
        m_value = next;
        refreshDisplay();
        refreshButtons();
        emit valueChanged(m_value);
    }

    // Takes what was typed as the new value, or leaves the value as it is;
    // either way the field then shows the value, never text that was not
    // accepted.
    void commitText()
    {
        const int value = valueFromText(m_display->text(), m_min, m_max, m_value);
        const bool changed = value != m_value;
        m_value = value;
        refreshDisplay();
        refreshButtons();
        if (changed == true)
        {
            emit valueChanged(m_value);
        }
    }

    void refreshDisplay() const { m_display->setText(QString::number(m_value)); }

    void refreshButtons() const
    {
        m_upBtn->setEnabled(m_value < m_max);
        m_downBtn->setEnabled(m_value > m_min);
    }

    void refreshArrowIcons()
    {
        m_upBtn->setIcon(svgIcon(QStringLiteral(":/assets/chevron-up.svg")));
        m_downBtn->setIcon(svgIcon(QStringLiteral(":/assets/chevron-down.svg")));
    }

    QLineEdit*     m_display   = nullptr;
    QIntValidator* m_validator = nullptr;
    QPushButton*   m_upBtn     = nullptr;
    QPushButton*   m_downBtn   = nullptr;
    int m_min   = 0;
    int m_max   = DEFAULT_MAXIMUM;
    int m_value = 0;
};
