#pragma once

// gripSplitter.h
// A QSplitter whose handle makes it obvious that it can be dragged: the usual
// thin divider line with a grip in the middle, both highlighted in the accent
// color on hover and while dragging. Double-clicking the handle emits
// GripSplitter::handleDoubleClicked(), e.g. to restore a default size.

#include <QEnterEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QSplitter>
#include <QSplitterHandle>

class GripSplitterHandle : public QSplitterHandle
{
    Q_OBJECT

public:
    // Odd, so the 1 px line and the grip share an exact center pixel column
    // (or row, between widgets stacked vertically).
    static constexpr int HANDLE_WIDTH = 9;

    GripSplitterHandle(const Qt::Orientation orientation, QSplitter* parent)
        : QSplitterHandle(orientation, parent)
    {
    }

signals:
    void doubleClicked();

protected:
    bool event(QEvent* event) override
    {
        const bool handled = QSplitterHandle::event(event);
        // KDE's Breeze style turns on hover events for splitter handles when it
        // polishes them, and on hover lays an invisible "splitter proxy" over
        // the handle to widen its grab area. The proxy makes this handle see a
        // Leave as soon as the pointer moves (so the highlight flashes off) and
        // swallows double-clicks. This handle is wide enough to grab already,
        // so hover events are switched off again after every (re)polish, which
        // keeps the proxy from ever appearing. Enter and Leave, which the
        // highlight relies on, arrive without them.
        if (event->type() == QEvent::Polish || event->type() == QEvent::StyleChange)
        {
            setAttribute(Qt::WA_Hover, false);
        }
        return handled;
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const bool active = underMouse() == true || m_dragging == true;
        const QColor lineColor = palette().color(active ? QPalette::Highlight : QPalette::Mid);
        const QColor gripColor = palette().color(active ? QPalette::Highlight : QPalette::PlaceholderText);

        // Divider line along the middle of the handle, drawn as a filled
        // column (or row) so it stays a crisp 1 px instead of an antialiased
        // 2 px smear, and the grip over it, centered on the same pixels. A
        // horizontal splitter has its widgets side by side, so its line runs
        // top to bottom.
        QRectF grip;
        if (orientation() == Qt::Horizontal)
        {
            const int centerX = width() / 2;
            p.fillRect(centerX, 0, LINE_WIDTH, height(), lineColor);
            grip = QRectF(centerX + LINE_WIDTH / 2.0 - GRIP_THICKNESS / 2.0,
                          (height() - GRIP_LENGTH) / 2.0,
                          GRIP_THICKNESS, GRIP_LENGTH);
        }
        else
        {
            const int centerY = height() / 2;
            p.fillRect(0, centerY, width(), LINE_WIDTH, lineColor);
            grip = QRectF((width() - GRIP_LENGTH) / 2.0,
                          centerY + LINE_WIDTH / 2.0 - GRIP_THICKNESS / 2.0,
                          GRIP_LENGTH, GRIP_THICKNESS);
        }
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(gripColor);
        p.drawRoundedRect(grip, GRIP_THICKNESS / 2.0, GRIP_THICKNESS / 2.0);
    }

    void enterEvent(QEnterEvent* event) override
    {
        QSplitterHandle::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent* event) override
    {
        QSplitterHandle::leaveEvent(event);
        update();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            m_dragging = true;
            update();
        }
        QSplitterHandle::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            m_dragging = false;
            update();
        }
        QSplitterHandle::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            emit doubleClicked();
            event->accept();
            return;
        }
        QSplitterHandle::mouseDoubleClickEvent(event);
    }

private:
    static constexpr int GRIP_THICKNESS = 5;
    static constexpr int GRIP_LENGTH    = 36;
    static constexpr int LINE_WIDTH     = 1;

    bool m_dragging = false;
};

class GripSplitter : public QSplitter
{
    Q_OBJECT

public:
    explicit GripSplitter(QWidget* parent = nullptr)
        : GripSplitter(Qt::Horizontal, parent)
    {
    }

    explicit GripSplitter(const Qt::Orientation orientation, QWidget* parent = nullptr)
        : QSplitter(orientation, parent)
    {
        // Set explicitly: it takes precedence over the 1 px width the global
        // QSplitter::handle rule in the stylesheets gives other splitters.
        setHandleWidth(GripSplitterHandle::HANDLE_WIDTH);
    }

signals:
    void handleDoubleClicked();

protected:
    QSplitterHandle* createHandle() override
    {
        GripSplitterHandle* handle = new GripSplitterHandle(orientation(), this);
        connect(handle, &GripSplitterHandle::doubleClicked, this, &GripSplitter::handleDoubleClicked);
        return handle;
    }
};
