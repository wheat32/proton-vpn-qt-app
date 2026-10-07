#pragma once

#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSvgRenderer>

// This is a styled replacement for QSpinBox that
//  renders its up/down chevrons from SVG resources correctly,
//  even when the SVGs use fill="currentColor".

class NumberSpinner : public QFrame
{
    Q_OBJECT

public:
    explicit NumberSpinner(QWidget* parent = nullptr) : QFrame(parent)
    {
        setObjectName(QStringLiteral("numberSpinner"));
        setFixedWidth(64);
        setFixedHeight(38);

        QHBoxLayout* outer = new QHBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        m_display = new QLineEdit(this);
        m_display->setAlignment(Qt::AlignCenter);
        m_display->setObjectName(QStringLiteral("numberSpinnerDisplay"));
        m_display->setValidator(new QIntValidator(m_min, m_max, m_display));
        outer->addWidget(m_display, 1);

        QFrame* btnFrame = new QFrame(this);
        btnFrame->setObjectName(QStringLiteral("numberSpinnerButtons"));
        QVBoxLayout* btnLayout = new QVBoxLayout(btnFrame);
        btnLayout->setContentsMargins(1, 0, 0, 0); // 1 px reveals the separator border
        btnLayout->setSpacing(0);

        m_upBtn   = makeArrowBtn(QStringLiteral(":/assets/chevron-up.svg"));
        m_downBtn = makeArrowBtn(QStringLiteral(":/assets/chevron-down.svg"));
        // Distinct names so QSS can round the correct corners of each button.
        m_upBtn->setObjectName(QStringLiteral("numberSpinnerBtnUp"));
        m_downBtn->setObjectName(QStringLiteral("numberSpinnerBtnDown"));
        btnLayout->addWidget(m_upBtn);
        btnLayout->addWidget(m_downBtn);
        outer->addWidget(btnFrame);

        connect(m_upBtn,   &QPushButton::clicked, this, [this] { step(+1); });
        connect(m_downBtn, &QPushButton::clicked, this, [this] { step(-1); });

        connect(m_display, &QLineEdit::editingFinished, this, [this]() {
            bool ok = false;
            const int val = m_display->text().toInt(&ok);
            if (ok)
            {
                const int clamped = qBound(m_min, val, m_max);
                if (clamped != m_value)
                {
                    m_value = clamped;
                    refreshDisplay();
                    refreshButtons();
                    emit valueChanged(m_value);
                }
            }
            else
            {
                refreshDisplay(); // revert to last valid value
            }
        });

        refreshDisplay();
    }

    void setRange(const int min, const int max)
    {
        m_min = min;
        m_max = max;
        m_value = qBound(m_min, m_value, m_max);

        if (QIntValidator* v = qobject_cast<QIntValidator*>(
                const_cast<QValidator*>(m_display->validator())))
        {
            v->setRange(m_min, m_max);
        }

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

private:
    // Load an SVG from a Qt resource, patch currentColor to the current palette
    // text color, and return a QIcon.  Called at construction and on palette change.
    static QIcon svgIcon(const QString& resource)
    {
        QFile file(resource);
        if (file.open(QIODevice::ReadOnly) == false)
            return {};

        QByteArray data = file.readAll();
        const QColor textColor = QGuiApplication::palette().color(QPalette::WindowText);
        data.replace("currentColor", textColor.name().toLatin1());

        QSvgRenderer renderer(data);
        QPixmap pixmap(10, 10);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        return QIcon(pixmap);
    }

    QPushButton* makeArrowBtn(const QString& resource)
    {
        QPushButton* btn = new QPushButton(this);
        btn->setIcon(svgIcon(resource));
        btn->setIconSize({10, 10});
        btn->setFixedWidth(24);
        btn->setFixedHeight(19); // half of the spinner's fixed 38 px height
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        return btn;
    }

    void step(const int delta)
    {
        const int next = m_value + delta;
        if (next < m_min || next > m_max)
            return;
        m_value = next;
        refreshDisplay();
        refreshButtons();
        emit valueChanged(m_value);
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

    QLineEdit*   m_display  = nullptr;
    QPushButton* m_upBtn    = nullptr;
    QPushButton* m_downBtn  = nullptr;
    int m_min   =  0;
    int m_max   = 99;
    int m_value =  0;
};

