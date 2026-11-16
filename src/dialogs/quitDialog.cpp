#include "quitDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
constexpr int QUIT_DIALOG_MIN_WIDTH       = 440;
constexpr int QUIT_DIALOG_MIN_HEIGHT      = 220;
constexpr int QUIT_DIALOG_SPACING         = 16;
constexpr int QUIT_DIALOG_MARGIN          = 24;
// +10px over the content margin so the dialog's layout-driven minimum height
// never renders content flush against the bottom edge.
constexpr int QUIT_DIALOG_BOTTOM_MARGIN   = 30;
constexpr int QUIT_DIALOG_BTN_ROW_SPACING = 8;
} // namespace

QuitDialog::QuitDialog(const bool portForwardingActive, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Quit ProtonVPN"));
    // No WA_DeleteOnClose: callers create this dialog on the stack and read its
    // result after exec() returns, so it must not schedule its own deletion.
    setModal(true);
    setMinimumWidth(QUIT_DIALOG_MIN_WIDTH);
    setMinimumHeight(QUIT_DIALOG_MIN_HEIGHT);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(QUIT_DIALOG_SPACING);
    layout->setContentsMargins(QUIT_DIALOG_MARGIN, QUIT_DIALOG_MARGIN, QUIT_DIALOG_MARGIN, QUIT_DIALOG_BOTTOM_MARGIN);

    QLabel* msgLabel = new QLabel(
        QStringLiteral("%1<br>%2").arg(
            tr("The VPN is currently active.").toHtmlEscaped(),
            tr("What would you like to do before quitting?").toHtmlEscaped()),
        this);
    msgLabel->setWordWrap(true);
    msgLabel->setTextFormat(Qt::RichText);
    layout->addWidget(msgLabel);

    if (portForwardingActive)
    {
        QLabel* pfLabel = new QLabel(
            QStringLiteral("<i>%1</i>").arg(
                tr("Note: the forwarded port lease will lapse shortly after "
                   "the app closes, as the keep-alive loop will no longer be running.")
                .toHtmlEscaped()),
            this);
        pfLabel->setWordWrap(true);
        pfLabel->setTextFormat(Qt::RichText);
        layout->addWidget(pfLabel);
    }

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(QUIT_DIALOG_BTN_ROW_SPACING);

    QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);
    cancelBtn->setObjectName(QStringLiteral("secondaryButton"));

    QPushButton* leaveOnBtn = new QPushButton(tr("Leave VPN on"), this);
    leaveOnBtn->setObjectName(QStringLiteral("leaveVpnOnButton"));
    leaveOnBtn->setDefault(true);

    QPushButton* disconnectBtn = new QPushButton(tr("Disconnect VPN"), this);
    disconnectBtn->setObjectName(QStringLiteral("dangerButton"));

    // Uniform size: same height, equal width via stretch, reduced horizontal
    // padding so the longest label ("Disconnect VPN") fits without clipping.
    const QString overridePadding = QStringLiteral("padding-left: 8px; padding-right: 8px;");
    cancelBtn->setStyleSheet(
        QStringLiteral("QPushButton#secondaryButton { %1 }").arg(overridePadding));
    leaveOnBtn->setStyleSheet(
        QStringLiteral("QPushButton#leaveVpnOnButton { %1 }").arg(overridePadding));
    disconnectBtn->setStyleSheet(
        QStringLiteral("QPushButton#dangerButton { %1 }").arg(overridePadding));

    const int btnH = disconnectBtn->sizeHint().height();
    cancelBtn->setFixedHeight(btnH);
    leaveOnBtn->setFixedHeight(btnH);
    disconnectBtn->setFixedHeight(btnH);

    btnRow->addWidget(cancelBtn, 1);
    btnRow->addWidget(leaveOnBtn, 1);
    btnRow->addWidget(disconnectBtn, 1);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(leaveOnBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(disconnectBtn, &QPushButton::clicked, this, [this]()
    {
        done(DisconnectResult);
    });
}
