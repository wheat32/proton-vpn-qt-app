#include "aboutDialog.h"

#include "../appVersion.h"
#include "../cli/platformUtils.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVersionNumber>
#include <QVBoxLayout>

namespace
{
constexpr int ABOUT_MIN_WIDTH          = 520;
constexpr int ABOUT_MIN_HEIGHT         = 480;
constexpr int ABOUT_LAYOUT_SPACING     = 10;
constexpr int VERSION_GRID_TOP_MARGIN  = 4;
constexpr int VERSION_GRID_BOT_MARGIN  = 8;
constexpr int VERSION_GRID_H_SPACING   = 8;
constexpr int VERSION_GRID_V_SPACING   = 4;
constexpr int VERSION_COL_STRETCH      = 1;
// The version table: one row per fact, its name in column 0.
constexpr int VERSION_VALUE_COL        = 1;
constexpr int VERSION_ROW_APP          = 0;
constexpr int VERSION_ROW_TESTED_CLI   = 1;
constexpr int VERSION_ROW_QT           = 2;
constexpr int VERSION_ROW_PACKAGE      = 3;
constexpr int VERSION_ROW_INSTALLED    = 4;
} // namespace

AboutDialog::AboutDialog(const QString& installedCliVersion, QWidget* parent)
    : QDialog(parent)
{
    // Versions from the embedded version.json resource
    const AppVersion::Info& version = AppVersion::embedded();
    const QString appVersion    = version.appVersion.isEmpty() == true ? QStringLiteral("unknown")
                                                                       : version.appVersion;
    const QString cliVersionMin = version.cliTestedMin;
    const QString cliVersionMax = version.cliTestedMax;

    // Build the "min-max" display string, e.g. "1.0.0-1.0.1"
    QString testedVersionStr;
    if (cliVersionMin.isEmpty() == false && cliVersionMax.isEmpty() == false)
    {
        testedVersionStr = cliVersionMin + QStringLiteral("-") + cliVersionMax;
    }
    else if (cliVersionMin.isEmpty() == false)
    {
        testedVersionStr = cliVersionMin;
    }
    else if (cliVersionMax.isEmpty() == false)
    {
        testedVersionStr = cliVersionMax;
    }
    else
    {
        testedVersionStr = QStringLiteral("unknown");
    }

    setWindowTitle(tr("About ProtonVPN Qt App"));
    setMinimumSize(ABOUT_MIN_WIDTH, ABOUT_MIN_HEIGHT);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(ABOUT_LAYOUT_SPACING);

    //  Title / subtitle
    QLabel* titleLabel = new QLabel(
        QStringLiteral("<h2 style=\"margin-bottom:2px;\">%1</h2>")
            .arg(tr("ProtonVPN Qt App").toHtmlEscaped()),
        this);
    titleLabel->setTextFormat(Qt::RichText);
    layout->addWidget(titleLabel);

    QLabel* subtitleLabel = new QLabel(
        QStringLiteral("<span style=\"color:#888;\">%1</span>")
            .arg(tr("A community-built Qt front-end for the Proton VPN CLI.").toHtmlEscaped()),
        this);
    subtitleLabel->setTextFormat(Qt::RichText);
    layout->addWidget(subtitleLabel);

    //  Version table
    QWidget*      versionWidget = new QWidget(this);
    QGridLayout*  versionGrid   = new QGridLayout(versionWidget);
    versionGrid->setContentsMargins(0, VERSION_GRID_TOP_MARGIN, 0, VERSION_GRID_BOT_MARGIN);
    versionGrid->setHorizontalSpacing(VERSION_GRID_H_SPACING);
    versionGrid->setVerticalSpacing(VERSION_GRID_V_SPACING);
    versionGrid->setColumnStretch(VERSION_VALUE_COL, VERSION_COL_STRETCH);

    auto makeKey = [&](const QString& text) -> QLabel*
    {
        QLabel* l = new QLabel(QStringLiteral("<b>%1</b>").arg(text.toHtmlEscaped()), versionWidget);
        l->setTextFormat(Qt::RichText);
        return l;
    };

    versionGrid->addWidget(makeKey(tr("App version:")), VERSION_ROW_APP, 0);
    versionGrid->addWidget(new QLabel(appVersion, versionWidget), VERSION_ROW_APP, VERSION_VALUE_COL);

    versionGrid->addWidget(makeKey(tr("Tested against CLI:")), VERSION_ROW_TESTED_CLI, 0);
    versionGrid->addWidget(new QLabel(testedVersionStr, versionWidget),
                           VERSION_ROW_TESTED_CLI, VERSION_VALUE_COL);

    versionGrid->addWidget(makeKey(tr("Qt version:")), VERSION_ROW_QT, 0);
    versionGrid->addWidget(new QLabel(QStringLiteral(QT_VERSION_STR), versionWidget),
                           VERSION_ROW_QT, VERSION_VALUE_COL);

    versionGrid->addWidget(makeKey(tr("Package:")), VERSION_ROW_PACKAGE, 0);
    versionGrid->addWidget(new QLabel(packageTypeName(), versionWidget),
                           VERSION_ROW_PACKAGE, VERSION_VALUE_COL);

    // Installed CLI version – highlighted only when outside the tested range
    if (installedCliVersion.isEmpty() == false)
    {
        const QVersionNumber installed = QVersionNumber::fromString(installedCliVersion);
        const QVersionNumber verMin    = QVersionNumber::fromString(cliVersionMin);
        const QVersionNumber verMax    = QVersionNumber::fromString(cliVersionMax);

        const bool tooOld  = installed.isNull() == false && verMin.isNull() == false
                             && installed < verMin;
        const bool tooNew  = installed.isNull() == false && verMax.isNull() == false
                             && installed > verMax;

        if (tooOld || tooNew)
        {
            const QString arrow   = tooNew == true ? QStringLiteral(" \u25b2") : QStringLiteral(" \u25bc");
            const QString color   = tooNew == true ? QStringLiteral("#f59e0b") : QStringLiteral("#ef4444");
            const QString tooltip = tooNew == true
                ? tr("Your installed CLI (v%1) is newer than the tested range (%2). "
                     "Things may work fine, but you could encounter unexpected behavior.")
                      .arg(installedCliVersion, testedVersionStr)
                : tr("Your installed CLI (v%1) is older than the tested range (%2). "
                     "Some features may not work correctly. Consider upgrading the CLI.")
                      .arg(installedCliVersion, testedVersionStr);

            QLabel* valLabel = new QLabel(installedCliVersion + arrow, versionWidget);
            valLabel->setStyleSheet(
                QStringLiteral("color: %1; font-weight: bold;").arg(color));
            valLabel->setToolTip(tooltip);

            versionGrid->addWidget(makeKey(tr("Installed CLI:")), VERSION_ROW_INSTALLED, 0);
            versionGrid->addWidget(valLabel, VERSION_ROW_INSTALLED, VERSION_VALUE_COL);
        }
    }

    layout->addWidget(versionWidget);

    //  Disclaimer + Credits
    QTextBrowser* browser = new QTextBrowser(this);
    browser->setOpenExternalLinks(true);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setHtml(
        QStringLiteral(
            "<p><b>\u26a0 %1</b> %2</p>"
            "<hr/>"
            "<h3>%3</h3>"
            "<ul><li>Nicholas Page (<a href='https://github.com/wheat32'>wheat32</a>)</li></ul>"
            "<h3>%4</h3>"
            "<ul>"
            "<li>%5</li>"
            "<li>%6</li>"
            "<li>%7</li>"
            "<li>%8</li>"
            "<li>%9</li>"
            "</ul>"
            "<hr/>"
            "<p style='color:#888;font-size:small;'>%10</p>")
        .arg(
            tr("Disclaimer:"),
            tr("This project is <b>not affiliated with, endorsed by, or supported by Proton AG</b> "
               "in any way. ProtonVPN and the Proton logo are trademarks of Proton AG."),
            tr("Author"),
            tr("Credits & Acknowledgements"),
            tr("Built with <a href='https://www.qt.io/'>Qt 6</a>"),
            tr("Uses the <a href='https://protonvpn.com/support/linux-vpn-tool/'>ProtonVPN Linux CLI</a>"),
            tr("Icons from <a href='https://icons.getbootstrap.com/'>Bootstrap Icons</a> (MIT License)"),
            tr("Country flag SVGs from <a href='https://github.com/lipis/flag-icons'>flag-icons</a>"
               " by Panayiotis Lipiridis (MIT License)"),
            tr("Globe map data from <a href='https://www.naturalearthdata.com/'>Natural Earth</a> (public domain)"),
            tr("This software is provided as-is, without warranty of any kind. Use at your own risk.")));
    layout->addWidget(browser);

    // Styled from the stylesheet (#dialogButton) instead of being left to the
    // widget style, which Breeze and Fusion (used by the AppImage) draw very
    // differently. The button is created here rather than taken from a
    // standard QDialogButtonBox::Close: the box polishes its own buttons as it
    // creates them, so an object name set afterwards never picks up the QSS rule.
    QPushButton* closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setObjectName(QStringLiteral("dialogButton"));
    closeBtn->setCursor(Qt::PointingHandCursor);

    QDialogButtonBox* btns = new QDialogButtonBox(this);
    btns->addButton(closeBtn, QDialogButtonBox::RejectRole);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(btns);
}
