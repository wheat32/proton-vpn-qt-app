#pragma once

#include <QEvent>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QTimer>

// Floating toast that shows a short message near the bottom of the parent window
// and disappears on its own after a configurable duration. A close button lets
// the user dismiss it early. Use the popup() factory - it handles lifetime
// automatically by calling deleteLater once the fade-out animation finishes.
class ToastNotification : public QFrame
{
    static constexpr int DEFAULT_DURATION_MS  = 3000;
    static constexpr int TOAST_LEFT_MARGIN    = 12;
    static constexpr int TOAST_MARGIN         = 8;
    static constexpr int TOAST_MSG_STRETCH    = 1;
    static constexpr int TOAST_CLOSE_BTN_SIZE = 20;
    static constexpr int FADE_DURATION_MS     = 400;
    static constexpr int TOAST_BOTTOM_MARGIN  = 32;

    Q_OBJECT
public:
    // Convenience factory - create and show in one call.
    static void popup(const QWidget* anchor, const QString& message, const int durationMs = DEFAULT_DURATION_MS)
    {
        // self-managing lifetime via deleteLater in dismiss()
        new ToastNotification(message, anchor, durationMs);
    }

    explicit ToastNotification(const QString& message, const QWidget* anchor,
                               const int durationMs = DEFAULT_DURATION_MS)
        : QFrame(anchor != nullptr ? anchor->window() : nullptr)
    {
        setObjectName(QStringLiteral("toastNotification"));

        QHBoxLayout* hLayout = new QHBoxLayout(this);
        hLayout->setContentsMargins(TOAST_LEFT_MARGIN, TOAST_MARGIN, TOAST_MARGIN, TOAST_MARGIN);
        hLayout->setSpacing(TOAST_MARGIN);

        QLabel* iconLabel = new QLabel(QStringLiteral("✓"), this);
        iconLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold;"));
        hLayout->addWidget(iconLabel);

        QLabel* msgLabel = new QLabel(message, this);
        msgLabel->setWordWrap(false);
        hLayout->addWidget(msgLabel, TOAST_MSG_STRETCH);

        QPushButton* closeBtn = new QPushButton(QStringLiteral("✕"), this);
        closeBtn->setObjectName(QStringLiteral("toastCloseBtn"));
        closeBtn->setFixedSize(TOAST_CLOSE_BTN_SIZE, TOAST_CLOSE_BTN_SIZE);
        closeBtn->setFlat(true);
        closeBtn->setCursor(Qt::PointingHandCursor);
        connect(closeBtn, &QPushButton::clicked, this, &ToastNotification::dismiss);
        hLayout->addWidget(closeBtn);

        // Opacity effect for fade-out animation
        m_opacity = new QGraphicsOpacityEffect(this);
        m_opacity->setOpacity(1.0);
        setGraphicsEffect(m_opacity);

        m_fadeAnim = new QPropertyAnimation(m_opacity, "opacity", this);
        m_fadeAnim->setDuration(FADE_DURATION_MS);
        m_fadeAnim->setStartValue(1.0);
        m_fadeAnim->setEndValue(0.0);
        connect(m_fadeAnim, &QPropertyAnimation::finished, this, &QObject::deleteLater);

        // Auto-dismiss timer
        QTimer* autoTimer = new QTimer(this);
        autoTimer->setSingleShot(true);
        connect(autoTimer, &QTimer::timeout, this, &ToastNotification::dismiss);
        autoTimer->start(durationMs);

        // Track parent window resizes so we can reposition
        if (parentWidget() != nullptr)
        {
            parentWidget()->installEventFilter(this);
        }

        QFrame::show();
        raise();

        // Defer sizing/positioning so the layout has been resolved
        QTimer::singleShot(0, this, [this]()
        {
            adjustSize();
            reposition();
        });
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == parentWidget() && event->type() == QEvent::Resize)
        {
            reposition();
        }

        return QFrame::eventFilter(watched, event);
    }

public slots:
    void dismiss()
    {
        if (m_dismissed == true) return;
        m_dismissed = true;
        m_fadeAnim->start();
    }

private:
    void reposition()
    {
        if (parentWidget() == nullptr) return;
        const QSize ps = parentWidget()->size();
        move((ps.width() - width()) / 2, ps.height() - height() - TOAST_BOTTOM_MARGIN);
    }

    QGraphicsOpacityEffect* m_opacity   = nullptr;
    QPropertyAnimation*     m_fadeAnim  = nullptr;
    bool                    m_dismissed = false;
};
