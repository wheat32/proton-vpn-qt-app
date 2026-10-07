#include "natpmpcDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QSvgRenderer>
#include <QVBoxLayout>

namespace
{
constexpr int NATPMPC_DLG_MIN_WIDTH  = 480;
constexpr int NATPMPC_DLG_SPACING    = 12;
constexpr int NATPMPC_DLG_MARGIN     = 20;
constexpr int NATPMPC_DLG_BOT_MARGIN = 16;
constexpr int NATPMPC_ICON_SIZE      = 32;
constexpr int NATPMPC_ICON_SPACING   = 8;
constexpr int NATPMPC_CMD_SPACING    = 4;
constexpr int NATPMPC_COPY_BTN_SIZE  = 28;
constexpr int NATPMPC_COPY_ICON_SIZE = 16;
constexpr int NATPMPC_BOTTOM_SPACING = 4;

QIcon makeClipboardIcon(const int size)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QSvgRenderer renderer(QStringLiteral(":/assets/clipboard2-plus.svg"));
    renderer.render(&p);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(pix.rect(), Qt::white);
    p.end();
    return {pix};
}
} // namespace

NatpmpcDialog::NatpmpcDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("natpmpc Not Installed"));
    setMinimumWidth(NATPMPC_DLG_MIN_WIDTH);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(NATPMPC_DLG_SPACING);
    layout->setContentsMargins(NATPMPC_DLG_MARGIN, NATPMPC_DLG_MARGIN,
                               NATPMPC_DLG_MARGIN, NATPMPC_DLG_BOT_MARGIN);

    QHBoxLayout* titleRow = new QHBoxLayout();
    QLabel* iconLabel = new QLabel(this);
    iconLabel->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning)
                             .pixmap(NATPMPC_ICON_SIZE, NATPMPC_ICON_SIZE));
    titleRow->addWidget(iconLabel);
    titleRow->addSpacing(NATPMPC_ICON_SPACING);
    QLabel* titleLabel = new QLabel(
        QStringLiteral("<b>%1</b>")
            .arg(tr("natpmpc is not installed.").toHtmlEscaped()),
        this);
    titleLabel->setTextFormat(Qt::RichText);
    titleRow->addWidget(titleLabel, 1);
    layout->addLayout(titleRow);

    QLabel* descLabel = new QLabel(
        tr("Port forwarding requires the <code>natpmpc</code> binary to display "
           "and keep the forwarded port alive. Without it, the forwarded port "
           "will not be shown in the app.<br><br>"
           "Install it using the command for your distribution:"),
        this);
    descLabel->setTextFormat(Qt::RichText);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    const QIcon clipIcon = makeClipboardIcon(NATPMPC_COPY_ICON_SIZE);

    auto addCmd = [&](const QString& distro, const QString& cmd)
    {
        layout->addWidget(new QLabel(distro, this));

        QWidget* row = new QWidget(this);
        QHBoxLayout* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(0, 0, 0, 0);
        hbox->setSpacing(NATPMPC_CMD_SPACING);

        QLineEdit* edit = new QLineEdit(cmd, row);
        edit->setReadOnly(true);
        edit->setObjectName(QStringLiteral("codeInput"));
        edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        hbox->addWidget(edit, 1);

        QPushButton* copyBtn = new QPushButton(row);
        copyBtn->setIcon(clipIcon);
        copyBtn->setIconSize({NATPMPC_COPY_ICON_SIZE, NATPMPC_COPY_ICON_SIZE});
        copyBtn->setFixedSize(NATPMPC_COPY_BTN_SIZE, NATPMPC_COPY_BTN_SIZE);
        copyBtn->setToolTip(tr("Copy to Clipboard"));
        copyBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  border: 1px solid palette(mid);"
            "  border-radius: 4px;"
            "  padding: 2px;"
            "  background: transparent;"
            "}"
            "QPushButton:hover {"
            "  border-color: palette(highlight);"
            "  background: rgba(255,255,255,15);"
            "}"
            "QPushButton:pressed {"
            "  background: rgba(255,255,255,30);"
            "}"));
        connect(copyBtn, &QPushButton::clicked, this, [cmd]()
        {
            QApplication::clipboard()->setText(cmd);
        });
        hbox->addWidget(copyBtn);
        layout->addWidget(row);
    };

    addCmd(tr("Debian / Ubuntu:"), QStringLiteral("sudo apt install natpmpc"));
    addCmd(tr("Fedora:"),          QStringLiteral("sudo dnf install libnatpmp"));
    addCmd(tr("Arch Linux:"),      QStringLiteral("sudo pacman -S libnatpmp"));

    QDialogButtonBox* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addSpacing(NATPMPC_BOTTOM_SPACING);
    layout->addWidget(btnBox);
}
