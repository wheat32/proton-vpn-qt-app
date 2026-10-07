#include "debugPage.h"

#include "../appConfig.h"
#include "../dialogs/errorDetailsDialog.h"
#include "../dialogs/updateAvailableDialog.h"
#include "../dialogs/whatsNewDialog.h"
#include "../migrations.h"

#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
// ReSharper disable once CppUnusedIncludeDirective
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <functional>

namespace
{
constexpr int DEBUG_TITLE_FONT_SIZE  = 14;
constexpr int DEBUG_H_MARGIN         = 24;
constexpr int DEBUG_V_MARGIN         = 20;
constexpr int DEBUG_LAYOUT_SPACING   = 12;
constexpr int CLEAR_BTN_WIDTH           = 160;
constexpr int CLEAR_ROW_SPACING         = 12;
constexpr int GRID_V_MARGIN             = 4;
constexpr int GRID_H_SPACING            = 12;
constexpr int GRID_V_SPACING            = 6;
constexpr int KEY_LABEL_MIN_WIDTH       = 180;
constexpr int RESET_BTN_WIDTH           = 64;
constexpr int DEFAULT_RECENT_COUNT      = 5;
constexpr int VALUE_COL_STRETCH         = 1;
constexpr int MIGRATION_VERSION_WIDTH   = 90;
constexpr int MIGRATION_BTN_WIDTH       = 80;
constexpr int SIGNIN_SCREEN_COLUMNS     = 2;

QLabel* makeSectionHeader(const QString& text, QWidget* parent)
{
    QLabel* lbl = new QLabel(text, parent);
    QFont f = lbl->font();
    f.setBold(true);
    lbl->setFont(f);
    return lbl;
}

QFrame* makeDivider(QWidget* parent)
{
    QFrame* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setObjectName(QStringLiteral("sidebarDivider"));
    return line;
}

QString boolStr(const bool v)
{
    return v == true ? QStringLiteral("true") : QStringLiteral("false");
}

QString themeStr(const AppConfig::Theme t)
{
    switch (t)
    {
        case AppConfig::Theme::Dark:
            return QStringLiteral("Dark");
        case AppConfig::Theme::Light:
            return QStringLiteral("Light");
        default:
            return QStringLiteral("System");
    }
}
} // namespace

DebugPage::DebugPage(QWidget* parent)
    : QWidget(parent)
{
    // Outer layout holds only the scroll area so the page itself never clips.
    m_outerLayout = new QVBoxLayout(this);
    m_outerLayout->setContentsMargins(0, 0, 0, 0);
    m_outerLayout->setSpacing(0);

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_outerLayout->addWidget(scroll);

    QWidget* content = new QWidget(scroll);
    scroll->setWidget(content);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setAlignment(Qt::AlignTop);
    layout->setSpacing(DEBUG_LAYOUT_SPACING);
    layout->setContentsMargins(DEBUG_H_MARGIN, DEBUG_V_MARGIN, DEBUG_H_MARGIN, DEBUG_V_MARGIN);

    //  Page header
    QLabel* titleLabel = new QLabel(tr("Debug Tools"), content);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(DEBUG_TITLE_FONT_SIZE);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    QLabel* subtitleLabel = new QLabel(
        tr("This page is only available in debug builds. Press F11 to toggle the sidebar button."),
        content);
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setObjectName(QStringLiteral("settingsDesc"));
    layout->addWidget(subtitleLabel);

    layout->addWidget(makeDivider(content));

    //  Dialogs section
    layout->addWidget(makeSectionHeader(tr("Dialogs"), content));

    QPushButton* whatsNewBtn = new QPushButton(tr("Test \u201cWhat\u2019s New\u201d Dialog"), content);
    whatsNewBtn->setObjectName(QStringLiteral("secondaryButton"));
    whatsNewBtn->setCursor(Qt::PointingHandCursor);
    connect(whatsNewBtn, &QPushButton::clicked, this, [this]()
    {
        QString version = QStringLiteral("?.?.?");
        QFile vf(QStringLiteral(":/version.json"));
        if (vf.open(QIODevice::ReadOnly))
        {
            const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
            vf.close();
            version = obj.value(QStringLiteral("app_version")).toString(version);
        }
        WhatsNewDialog* dlg = new WhatsNewDialog(version, this);
        dlg->setModal(true);
        dlg->show();
    });
    layout->addWidget(whatsNewBtn, 0, Qt::AlignLeft);

    QPushButton* errorDialogBtn = new QPushButton(tr("Test Error Details Dialog (overflow)"), content);
    errorDialogBtn->setObjectName(QStringLiteral("secondaryButton"));
    errorDialogBtn->setCursor(Qt::PointingHandCursor);
    connect(errorDialogBtn, &QPushButton::clicked, this, [this]()
    {
        // Long text designed to stress both vertical (many lines) and horizontal
        // (very long single line) overflow handling in the dialog.
        const QString loremLine = tr(
            "This is a very long error line intended to trigger horizontal scrollbar / wrapping behaviour "
            "inside the ErrorDetailsDialog; it just keeps going and going without any newline break whatsoever. "
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore "
            "et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi.");

        constexpr int ERROR_BODY_LINES = 38;
        QStringList lines;
        lines.reserve(ERROR_BODY_LINES + 2);
        lines << loremLine;  // first line: horizontal overflow
        for (int i = 1; i <= ERROR_BODY_LINES; ++i)
        {
            lines << tr("[line %1] Error: something went wrong in module %1. Details follow here.").arg(i);
        }
        lines << loremLine;  // last line: horizontal overflow again

        ErrorDetailsDialog* dlg = new ErrorDetailsDialog(lines.join(QLatin1Char('\n')), this);
        dlg->setModal(true);
        dlg->show();
    });
    layout->addWidget(errorDialogBtn, 0, Qt::AlignLeft);

    QPushButton* updateDialogBtn = new QPushButton(tr("Test \u201cUpdate Available\u201d Dialog"), content);
    updateDialogBtn->setObjectName(QStringLiteral("secondaryButton"));
    updateDialogBtn->setCursor(Qt::PointingHandCursor);
    connect(updateDialogBtn, &QPushButton::clicked, this, [this]()
    {
        QString currentVersion = QStringLiteral("?.?.?");
        QFile vf(QStringLiteral(":/version.json"));
        if (vf.open(QIODevice::ReadOnly))
        {
            const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
            vf.close();
            currentVersion = obj.value(QStringLiteral("app_version")).toString(currentVersion);
        }
        UpdateAvailableDialog* dlg = new UpdateAvailableDialog(currentVersion, QStringLiteral("99.0.0"), this);
        dlg->setModal(true);
        dlg->show();
    });
    layout->addWidget(updateDialogBtn, 0, Qt::AlignLeft);

    layout->addWidget(makeDivider(content));

    //  Pages section
    layout->addWidget(makeSectionHeader(tr("Pages"), content));

    QPushButton* notInstalledBtn = new QPushButton(tr("Test \u201cNot Installed\u201d Page"), content);
    notInstalledBtn->setObjectName(QStringLiteral("secondaryButton"));
    notInstalledBtn->setCursor(Qt::PointingHandCursor);
    notInstalledBtn->setToolTip(
        tr("Shows the page displayed when the protonvpn CLI cannot be found. "
           "Its Check Again button runs the real CLI check."));
    connect(notInstalledBtn, &QPushButton::clicked, this, &DebugPage::notInstalledPageRequested);
    layout->addWidget(notInstalledBtn, 0, Qt::AlignLeft);

    QPushButton* cliNotRespondingBtn =
        new QPushButton(tr("Test \u201cCLI Not Responding\u201d Page"), content);
    cliNotRespondingBtn->setObjectName(QStringLiteral("secondaryButton"));
    cliNotRespondingBtn->setCursor(Qt::PointingHandCursor);
    cliNotRespondingBtn->setToolTip(
        tr("Shows the page displayed when protonvpn info times out at startup. "
           "Its Try Again button runs the real login check."));
    connect(cliNotRespondingBtn, &QPushButton::clicked, this, &DebugPage::cliNotRespondingPageRequested);
    layout->addWidget(cliNotRespondingBtn, 0, Qt::AlignLeft);

    layout->addWidget(makeDivider(content));

    //  Home screen section
    // More on the home screen than it usually has, for checking its layout:
    // whether the dropdowns still fit below the content, and when they move.
    layout->addWidget(makeSectionHeader(tr("Home Screen"), content));
    {
        QWidget* row = new QWidget(content);
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(GRID_H_SPACING);

        QPushButton* warningBtn = new QPushButton(tr("Add Warning"), row);
        warningBtn->setObjectName(QStringLiteral("secondaryButton"));
        warningBtn->setCursor(Qt::PointingHandCursor);
        warningBtn->setToolTip(
            tr("Adds a sample warning to the home screen. Each click adds another, "
               "short and long in turn."));
        connect(warningBtn, &QPushButton::clicked, this, [this]()
        {
            const QStringList samples = {
                tr("<b>Test warning %1</b> from the Debug page."),
                tr("<b>Test warning %1</b> from the Debug page, long enough to wrap onto several "
                   "lines so the warnings can be checked with taller content. Dismiss it with "
                   "its close button or Clear All."),
            };
            const QString sample = samples.at(m_homeWarningCount % samples.size());
            ++m_homeWarningCount;
            emit homeWarningRequested(sample.arg(m_homeWarningCount));
        });
        rowLayout->addWidget(warningBtn);

        // Shows the error and hides it again, to compare the two layouts.
        QPushButton* errorBtn = new QPushButton(tr("Show Error"), row);
        errorBtn->setObjectName(QStringLiteral("secondaryButton"));
        errorBtn->setCursor(Qt::PointingHandCursor);
        errorBtn->setToolTip(
            tr("Puts the home screen into its error state with a sample CLI error, until "
               "hidden again or the connection state next changes."));
        connect(errorBtn, &QPushButton::clicked, this, [this, errorBtn]()
        {
            m_homeErrorShown = m_homeErrorShown == false;
            errorBtn->setText(m_homeErrorShown == true ? tr("Hide Error") : tr("Show Error"));
            if (m_homeErrorShown == false)
            {
                emit homeErrorHidden();
                return;
            }
            // Read as a CLI failure, like a real traceback: the home screen
            // then also suggests signing out and back in.
            emit homeErrorRequested(QStringLiteral(
                "Traceback (most recent call last):\n"
                "  File \"/usr/bin/protonvpn\", line 8, in <module>\n"
                "    sys.exit(main())\n"
                "RuntimeError: Test error from the Debug page."));
        });
        rowLayout->addWidget(errorBtn);

        layout->addWidget(row, 0, Qt::AlignLeft);
    }

    layout->addWidget(makeDivider(content));

    //  Sign-in screens section
    // Every screen the second factor can show, with the error it would carry,
    // for reviewing the design without a 2FA account or a security key.
    layout->addWidget(makeSectionHeader(tr("Sign-in Screens"), content));
    {
        struct Screen
        {
            QString name;
            SigninPrompt prompt;
            QString error;
        };
        const QList<Screen> screens = {
            {tr("Authenticator Code"), SigninPrompt::Code, QString()},
            {tr("Code Rejected"), SigninPrompt::Code, signinRejectedMessage(SigninPrompt::Code)},
            {tr("Key: Waiting"), SigninPrompt::SecurityKey, QString()},
            {tr("Key: Touch"), SigninPrompt::SecurityKeyTouch, QString()},
            {tr("Key: PIN"), SigninPrompt::SecurityKeyPin, QString()},
            {tr("Key: Wrong PIN"), SigninPrompt::SecurityKeyPin, signinRejectedMessage(SigninPrompt::SecurityKeyPin)},
            {tr("Key: Not Detected"), SigninPrompt::SecurityKeyMissing, QString()},
            {tr("Key: Multiple Keys"), SigninPrompt::SecurityKeyChoose, QString()},
            {tr("Key: Signing In"), SigninPrompt::SecurityKeyRead, QString()},
            // The CLI's own message for a key that could not be used.
            {tr("Key: Failed"), SigninPrompt::SecurityKeyFailed,
             QStringLiteral("Security key authentication failed. Please try again.")},
        };

        QWidget* gridWidget = new QWidget(content);
        QGridLayout* grid = new QGridLayout(gridWidget);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(GRID_H_SPACING);
        grid->setVerticalSpacing(GRID_V_SPACING);
        for (qsizetype i = 0; i < screens.size(); ++i)
        {
            const Screen& screen = screens.at(i);
            QPushButton* btn = new QPushButton(screen.name, gridWidget);
            btn->setObjectName(QStringLiteral("secondaryButton"));
            btn->setCursor(Qt::PointingHandCursor);
            connect(btn, &QPushButton::clicked, this, [this, screen]()
            {
                emit signinScreenRequested(screen.prompt, screen.error);
            });
            grid->addWidget(btn, static_cast<int>(i / SIGNIN_SCREEN_COLUMNS),
                            static_cast<int>(i % SIGNIN_SCREEN_COLUMNS));
        }
        layout->addWidget(gridWidget, 0, Qt::AlignLeft);
    }

    layout->addWidget(makeDivider(content));

    //  Settings section
    layout->addWidget(makeSectionHeader(tr("Settings"), content));

    // "Clear All Settings" row
    QHBoxLayout* clearRow = new QHBoxLayout();
    clearRow->setSpacing(CLEAR_ROW_SPACING);

    QLabel* clearDesc = new QLabel(
        tr("Delete the config file and reset every value to its default."),
        content);
    clearDesc->setWordWrap(true);
    clearDesc->setObjectName(QStringLiteral("settingsDesc"));
    clearRow->addWidget(clearDesc, 1);

    QPushButton* clearAllBtn = new QPushButton(tr("Clear All Settings"), content);
    clearAllBtn->setObjectName(QStringLiteral("dangerButton"));
    clearAllBtn->setCursor(Qt::PointingHandCursor);
    clearAllBtn->setFixedWidth(CLEAR_BTN_WIDTH);
    connect(clearAllBtn, &QPushButton::clicked, this, [this]()
    {
        QMessageBox mb(this);
        mb.setWindowTitle(tr("Clear All Settings"));
        mb.setText(tr("This will delete the config file and reset every setting to its default. "
                      "The change takes effect immediately in memory."));
        mb.setIcon(QMessageBox::Warning);
        mb.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        mb.setDefaultButton(QMessageBox::Cancel);
        if (mb.exec() == QMessageBox::Yes)
        {
            AppConfig::instance().resetToDefaults();
            refreshValues();
        }
    });
    clearRow->addWidget(clearAllBtn, 0, Qt::AlignRight);
    layout->addLayout(clearRow);

    //  Per-key grid
    QWidget* gridWidget = new QWidget(content);
    QGridLayout* grid = new QGridLayout(gridWidget);
    grid->setContentsMargins(0, GRID_V_MARGIN, 0, GRID_V_MARGIN);
    grid->setHorizontalSpacing(GRID_H_SPACING);
    grid->setVerticalSpacing(GRID_V_SPACING);
    grid->setColumnStretch(1, VALUE_COL_STRETCH); // value column stretches

    // Appends one row, wires the Reset button via onReset, and returns
    // the value label so the caller can update it in refreshValues().
    int row = 0;
    auto addRow = [&](const QString& key,
                      const QString& defaultHint,
                      std::function<void()> onReset) -> QLabel*
    {
        QLabel* keyLbl = new QLabel(
            QStringLiteral("<b>%1</b>").arg(key.toHtmlEscaped()),
            gridWidget);
        keyLbl->setTextFormat(Qt::RichText);
        keyLbl->setMinimumWidth(KEY_LABEL_MIN_WIDTH);

        QLabel* valLbl = new QLabel(gridWidget);
        valLbl->setTextFormat(Qt::PlainText);
        valLbl->setStyleSheet(QStringLiteral("font-family: monospace;"));

        QPushButton* resetBtn = new QPushButton(tr("Reset"), gridWidget);
        resetBtn->setObjectName(QStringLiteral("secondaryButton"));
        resetBtn->setCursor(Qt::PointingHandCursor);
        resetBtn->setToolTip(tr("Reset to default: %1").arg(defaultHint));
        resetBtn->setFixedWidth(RESET_BTN_WIDTH);
        connect(resetBtn, &QPushButton::clicked, this, [this, onReset]()
        {
            onReset();
            refreshValues();
        });

        grid->addWidget(keyLbl,   row, 0);
        grid->addWidget(valLbl,   row, 1);
        grid->addWidget(resetBtn, row, 2);
        ++row;

        return valLbl;
    };

    m_valAutoConnect = addRow(
        QStringLiteral("auto_connect"),
        QStringLiteral("false"),
        []() { AppConfig::instance().setAutoConnect(false); });

    m_valNotifications = addRow(
        QStringLiteral("notifications"),
        QStringLiteral("true"),
        []() { AppConfig::instance().setNotifications(true); });

    m_valRecentCount = addRow(
        QStringLiteral("recent_connections_count"),
        QStringLiteral("5"),
        []() { AppConfig::instance().setRecentConnectionsCount(DEFAULT_RECENT_COUNT); });

    m_valStartHidden = addRow(
        QStringLiteral("start_hidden"),
        QStringLiteral("false"),
        []() { AppConfig::instance().setStartHidden(false); });

    m_valShowPicker = addRow(
        QStringLiteral("show_location_picker"),
        QStringLiteral("true"),
        []() { AppConfig::instance().setShowLocationPicker(true); });

    m_valTheme = addRow(
        QStringLiteral("theme"),
        QStringLiteral("System"),
        []() { AppConfig::instance().setTheme(AppConfig::Theme::System); });

    m_valLastSeenVersion = addRow(
        QStringLiteral("last_seen_version"),
        QStringLiteral("(empty)"),
        []() { AppConfig::instance().setLastSeenVersion(QString()); });

    m_valCheckForUpdates = addRow(
        QStringLiteral("check_for_updates"),
        QStringLiteral("true"),
        []() { AppConfig::instance().setCheckForUpdates(true); });

    m_valLogToFile = addRow(
        QStringLiteral("log_to_file"),
        QStringLiteral("false"),
        []() { AppConfig::instance().setLogToFile(false); });

    layout->addWidget(gridWidget);

#ifdef QT_DEBUG
    layout->addWidget(makeDivider(content));

    // Migrations section
    layout->addWidget(makeSectionHeader(tr("Migrations"), content));

    QLabel* migrDesc = new QLabel(
        tr("Dry-run tests for one-time upgrade migrations. "
           "Set the simulated previous version to control which migrations would fire."),
        content);
    migrDesc->setWordWrap(true);
    migrDesc->setObjectName(QStringLiteral("settingsDesc"));
    layout->addWidget(migrDesc);

    // Version input row + Run All button
    QHBoxLayout* versionRow = new QHBoxLayout();
    versionRow->setSpacing(CLEAR_ROW_SPACING);
    versionRow->addWidget(new QLabel(tr("Simulate previous version:"), content));

    m_migrationVersionInput = new QLineEdit(QStringLiteral("1.0.0"), content);
    m_migrationVersionInput->setObjectName(QStringLiteral("inputField"));
    m_migrationVersionInput->setFixedWidth(MIGRATION_VERSION_WIDTH);
    versionRow->addWidget(m_migrationVersionInput);

    versionRow->addStretch();

    QPushButton* runAllBtn = new QPushButton(tr("Run All"), content);
    runAllBtn->setObjectName(QStringLiteral("secondaryButton"));
    runAllBtn->setCursor(Qt::PointingHandCursor);
    runAllBtn->setFixedWidth(MIGRATION_BTN_WIDTH);
    connect(runAllBtn, &QPushButton::clicked, this, [this]()
    {
        const QString result = Migrations::testAll(m_migrationVersionInput->text().trimmed());
        QMessageBox::information(this, tr("Migration Test: Run All"), result);
    });
    versionRow->addWidget(runAllBtn);
    layout->addLayout(versionRow);

    // One row per migration: [name in code font] [Run Test button]
    auto addMigrationRow = [&](const QString& name, std::function<QString()> runTest)
    {
        QHBoxLayout* row = new QHBoxLayout();
        QLabel* lbl = new QLabel(
            QStringLiteral("<code>%1</code>").arg(name.toHtmlEscaped()), content);
        lbl->setTextFormat(Qt::RichText);
        row->addWidget(lbl, 1);

        QPushButton* btn = new QPushButton(tr("Run Test"), content);
        btn->setObjectName(QStringLiteral("secondaryButton"));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedWidth(MIGRATION_BTN_WIDTH);
        connect(btn, &QPushButton::clicked, this, [this, name, runTest]()
        {
            QMessageBox::information(this,
                tr("Migration Test: %1").arg(name),
                runTest());
        });
        row->addWidget(btn);
        layout->addLayout(row);
    };

    addMigrationRow(
        QStringLiteral("migrateSystemdToXdgAutostart"),
        [this]()
        {
            return Migrations::testMigrateSystemdToXdgAutostart(
                m_migrationVersionInput->text().trimmed());
        });
#endif // QT_DEBUG

    layout->addStretch();

    // Populate value labels on first show.
    refreshValues();
}

void DebugPage::refreshValues() const
{
    const AppConfig& cfg = AppConfig::instance();

    m_valAutoConnect->setText(boolStr(cfg.autoConnect()));
    m_valNotifications->setText(boolStr(cfg.notifications()));
    m_valRecentCount->setText(QString::number(cfg.recentConnectionsCount()));
    m_valStartHidden->setText(boolStr(cfg.startHidden()));
    m_valShowPicker->setText(boolStr(cfg.showLocationPicker()));
    m_valTheme->setText(themeStr(cfg.theme()));

    const QString lsv = cfg.lastSeenVersion();
    m_valLastSeenVersion->setText(lsv.isEmpty() ? QStringLiteral("(empty)") : lsv);
    m_valCheckForUpdates->setText(boolStr(cfg.checkForUpdates()));
    m_valLogToFile->setText(boolStr(cfg.logToFile()));
}

void DebugPage::setLeftPadding(int px)
{
    auto m = m_outerLayout->contentsMargins();
    m.setLeft(px);
    m_outerLayout->setContentsMargins(m);
}
