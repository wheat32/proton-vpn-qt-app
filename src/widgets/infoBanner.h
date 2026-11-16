#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

// A named namespace rather than an anonymous one: this is a header, and an
// anonymous namespace would give every file that includes it its own copy.
namespace InfoBannerDetail
{
constexpr int BANNER_LEFT_MARGIN = 12;
constexpr int BANNER_MARGIN      = 8;
constexpr int DISMISS_BTN_SIZE   = 22;

// The amber warning palette.
constexpr const char* TEXT_COLOR       = "#7a5c00";
constexpr const char* BACKGROUND_COLOR = "#fff3cd";
constexpr const char* BORDER_COLOR     = "#e6ac00";
} // namespace InfoBannerDetail

// ============================================================
// InfoBanner – a dismissable horizontal warning/info banner.
//
// Usage:
//   InfoBanner* banner = new InfoBanner("Message text", parent);
//   layout->insertWidget(0, banner);
//
// The banner uses the standard amber warning palette by default.
// Call dismiss() programmatically or connect to dismissed() signal.
// The widget deletes itself after being dismissed.
// ============================================================
class InfoBanner : public QFrame
{
    Q_OBJECT
public:
    explicit InfoBanner(const QString& htmlMessage, QWidget* parent = nullptr)
        : QFrame(parent)
    {
        using namespace InfoBannerDetail;

        // The frame rule names the banner itself. A plain "QFrame" selector
        // also matches the labels inside it (QLabel is a QFrame), which then
        // each drew the banner's border around themselves.
        setStyleSheet(
            QStringLiteral(
                "InfoBanner { background-color: %1; border: 1px solid %2; border-radius: 6px; } "
                "InfoBanner QLabel { color: %3; background: transparent; border: none; }")
            .arg(QLatin1String(BACKGROUND_COLOR),
                 QLatin1String(BORDER_COLOR),
                 QLatin1String(TEXT_COLOR)));

        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(BANNER_LEFT_MARGIN, BANNER_MARGIN, BANNER_MARGIN, BANNER_MARGIN);
        layout->setSpacing(BANNER_MARGIN);

        QLabel* iconLabel = new QLabel(QStringLiteral("⚠"), this);
        iconLabel->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
        layout->addWidget(iconLabel, 0, Qt::AlignTop);

        QLabel* msgLabel = new QLabel(htmlMessage, this);
        msgLabel->setWordWrap(true);
        msgLabel->setTextFormat(Qt::RichText);
        msgLabel->setOpenExternalLinks(true);
        msgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(msgLabel, 1);

        QPushButton* dismissBtn = new QPushButton(QStringLiteral("✕"), this);
        dismissBtn->setFixedSize(DISMISS_BTN_SIZE, DISMISS_BTN_SIZE);
        dismissBtn->setFlat(true);
        dismissBtn->setCursor(Qt::PointingHandCursor);
        dismissBtn->setStyleSheet(
            QStringLiteral("QPushButton { color: %1; font-weight: bold; border: none; "
                           "background: transparent; } QPushButton:hover { opacity: 0.7; }")
                .arg(QLatin1String(TEXT_COLOR)));
        connect(dismissBtn, &QPushButton::clicked, this, &InfoBanner::dismiss);
        layout->addWidget(dismissBtn, 0, Qt::AlignTop);
    }

signals:
    void dismissed();

public slots:
    void dismiss()
    {
        emit dismissed();
        setVisible(false);
        deleteLater();
    }
};
