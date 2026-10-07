#include "reconnectDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
constexpr int RECONNECT_DLG_MIN_WIDTH  = 440;
constexpr int RECONNECT_DLG_SPACING    = 16;
constexpr int RECONNECT_DLG_H_MARGIN   = 24;
constexpr int RECONNECT_DLG_V_MARGIN   = 24;
constexpr int RECONNECT_DLG_BOT_MARGIN = 20;
constexpr int RECONNECT_BTN_SPACING    = 8;
} // namespace

ReconnectDialog::ReconnectDialog(const QString& settingLabel, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Reconnect Required"));
    setModal(true);
    setMinimumWidth(RECONNECT_DLG_MIN_WIDTH);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(RECONNECT_DLG_SPACING);
    layout->setContentsMargins(RECONNECT_DLG_H_MARGIN, RECONNECT_DLG_V_MARGIN,
                               RECONNECT_DLG_H_MARGIN, RECONNECT_DLG_BOT_MARGIN);

    QLabel* heading = new QLabel(
        QStringLiteral("<b>%1</b>")
            .arg(tr("%1: Reconnect Required").arg(settingLabel).toHtmlEscaped()),
        this);
    heading->setTextFormat(Qt::RichText);

    QLabel* body = new QLabel(
        tr("Changing this setting requires reconnecting to the VPN.\n\n"
           "You can disconnect, apply the change, and reconnect to the same "
           "location automatically, or dismiss this dialog and do it manually."),
        this);
    body->setWordWrap(true);

    layout->addWidget(heading);
    layout->addWidget(body);

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(RECONNECT_BTN_SPACING);

    QPushButton* dismissBtn = new QPushButton(tr("Dismiss"), this);
    dismissBtn->setObjectName(QStringLiteral("secondaryButton"));
    connect(dismissBtn, &QPushButton::clicked, this, &QDialog::reject);

    QPushButton* reconnectBtn = new QPushButton(tr("Apply && Reconnect"), this);
    reconnectBtn->setObjectName(QStringLiteral("primaryButton"));
    reconnectBtn->setDefault(true);
    connect(reconnectBtn, &QPushButton::clicked, this, &QDialog::accept);

    const int btnH = reconnectBtn->sizeHint().height();
    dismissBtn->setFixedHeight(btnH);
    reconnectBtn->setFixedHeight(btnH);

    btnRow->addWidget(dismissBtn, 1);
    btnRow->addWidget(reconnectBtn, 1);
    layout->addLayout(btnRow);
}

// static
bool ReconnectDialog::confirm(const QString& settingLabel, QWidget* parent)
{
    ReconnectDialog dlg(settingLabel, parent);
    return dlg.exec() == QDialog::Accepted;
}
