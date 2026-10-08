#include <QApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QLocale>
#include <QMessageBox>
#include <QStyleFactory>
#include <QSysInfo>
#include <QSystemTrayIcon>
#include <QTranslator>
#include "appConfig.h"
#include "motionPreference.h"
#include "cli/appImageUtils.h"
#include "cli/flatpakUtils.h"
#include "cli/platformUtils.h"
#include "dbus/vpnStatusAdaptor.h"
#include "debug.h"
#include "fileLogger.h"
#include "mainWindow.h"
#include "migrations.h"
#include "themeManager.h"

int main(int argc, char* argv[])
{
    const QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ProtonVPN"));
    QApplication::setApplicationDisplayName(QStringLiteral("ProtonVPN"));

    // Install translator for UI strings
    static QTranslator translator;
    const QLocale locale = QLocale::system();
    if (translator.load(locale, QStringLiteral("proton_vpn_qt"), QStringLiteral("_"), QStringLiteral(":/i18n")))
    {
        QApplication::installTranslator(&translator);
    }

    // Read version from the embedded version.json so there is a single source of truth.
    QString appVersion = QStringLiteral("unknown");
    QString cliVersionTestedMin = QStringLiteral("unknown");
    QString cliVersionTestedMax = QStringLiteral("unknown");
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly))
    {
        const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
        vf.close();
        if (obj.contains(QStringLiteral("app_version")))
        {
            appVersion = obj[QStringLiteral("app_version")].toString();
        }
        if (obj.contains(QStringLiteral("cli_version_tested_min")))
        {
            cliVersionTestedMin = obj[QStringLiteral("cli_version_tested_min")].toString();
        }
        if (obj.contains(QStringLiteral("cli_version_tested_max")))
        {
            cliVersionTestedMax = obj[QStringLiteral("cli_version_tested_max")].toString();
        }
    }
    QApplication::setApplicationVersion(appVersion);

    // Mirror DBG_* output to a rotating log file if the user has enabled it.
    FileLogger::instance().setEnabled(AppConfig::instance().logToFile());
    // --verbose (or -v) turns verbose output on for this run, whatever the
    // Verbose Logging setting says.
    const QStringList arguments = QApplication::arguments().mid(1);
    VerboseLog::setLaunchOption(arguments.contains(QStringLiteral("--verbose"))
                                || arguments.contains(QStringLiteral("-v")));

    //  Startup diagnostics
    DBG_APP(QStringLiteral("=== ProtonVPN Qt App starting ==="));
    DBG_APP(QStringLiteral("App version        : ") + appVersion);
    DBG_APP(QStringLiteral("CLI tested min     : ") + cliVersionTestedMin);
    DBG_APP(QStringLiteral("CLI tested max     : ") + cliVersionTestedMax);
    DBG_APP(QStringLiteral("Qt version         : ") + QString::fromLatin1(qVersion()));
    DBG_APP(QStringLiteral("Package type       : ") + packageTypeName());
    DBG_APP(QStringLiteral("OS                 : ") + QSysInfo::prettyProductName());
    DBG_APP(QStringLiteral("Kernel             : ") + QSysInfo::kernelVersion());
    DBG_APP(QStringLiteral("CPU arch           : ") + QSysInfo::currentCpuArchitecture());
    DBG_APP(QStringLiteral("Locale             : ") + QLocale::system().name());
    DBG_APP(QStringLiteral("Config dir         : ") + AppConfig::configDir());
    DBG_APP(QStringLiteral("Verbose output     : ")
            + (VerboseLog::launchOption() == true ? QStringLiteral("on (--verbose)")
               : AppConfig::instance().verboseLogging() == true ? QStringLiteral("on (setting)")
                                                                : QStringLiteral("off")));
    DBG_APP(QStringLiteral("================================="));

    AppConfig::instance().logLoadedConfig();

    // Use Breeze style on KDE Plasma if available, else Fusion
    const QStringList availableStyles = QStyleFactory::keys();
    if (availableStyles.contains(QStringLiteral("Breeze"), Qt::CaseInsensitive))
    {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Breeze")));
    }
    else if (availableStyles.contains(QStringLiteral("Fusion"), Qt::CaseInsensitive))
    {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    }

    // Apply theme (palette + stylesheet) based on saved preference. Done
    // before the startup checks below so their message boxes match the app
    // rather than showing Qt's default look (light, with stock buttons).
    ThemeManager::apply(AppConfig::instance().theme());

    // Single-instance guard - prevent multiple copies running at the same time.
    const QString lockPath = QDir::tempPath() + QStringLiteral("/proton-vpn-qt-app.lock");
    QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(0); // never treat a lock as stale
    constexpr int lockTimeoutMs = 100;
    if (lockFile.tryLock(lockTimeoutMs) == false)
    {
        qint64 pid = 0;
        QString hostname;
        QString appname;
        lockFile.getLockInfo(&pid, &hostname, &appname);
        DBG_APP(QStringLiteral("Another instance of ProtonVPN is already running (PID: ") +
                (pid > 0 ? QString::number(pid) : QStringLiteral("unknown")) +
                QStringLiteral("). Exiting."));
        QMessageBox::warning(
            nullptr,
            QCoreApplication::translate("main", "Already Running"),
            QCoreApplication::translate("main", "ProtonVPN is already running.\n\nCheck your system tray or taskbar."));
        return 1;
    }

    // The Standalone AppImage bundles its own ProtonVPN CLI. If a separate
    // CLI is also installed on the host, the two installs fight over the
    // same daemon/session state. Refuse to start rather than risk that.
    if (isStandaloneAppImage() && systemProtonVpnCliInstalledSeparately())
    {
        DBG_APP(QStringLiteral("Standalone AppImage detected a separate system ProtonVPN CLI install. Refusing to start."));
        QMessageBox::critical(
            nullptr,
            QCoreApplication::translate("main", "Conflicting ProtonVPN CLI Installation"),
            QCoreApplication::translate("main",
                "This AppImage bundles its own ProtonVPN CLI, but a separate ProtonVPN CLI "
                "installation was also found on your system. Running both together can cause "
                "conflicts.\n\nPlease either use the Lite AppImage instead, or uninstall the "
                "system ProtonVPN CLI."));
        return 1;
    }

    // Asks the desktop for its reduce-motion preference now, so the answer is
    // in before the VPN page decides whether to show its globe.
    MotionPreference::instance().start();

    // Run one-time upgrade migrations before the main window is constructed.
    // lastSeenVersion() still holds the previous version at this point.
    Migrations::run(AppConfig::instance().lastSeenVersion());

    MainWindow w;

    // Expose VPN status on the D-Bus session bus
    // The adaptor must be parented to the adapted object (VpnManager)
    const VpnStatusAdaptor* dbusAdaptor = new VpnStatusAdaptor(w.manager());
    Q_UNUSED(dbusAdaptor)
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.isConnected())
    {
        sessionBus.registerObject(QStringLiteral("/com/protonvpn/app"), w.manager());
        if (sessionBus.registerService(QStringLiteral("com.protonvpn.app")) == false)
        {
            DBG_APP(QStringLiteral("D-Bus: failed to register service com.protonvpn.app: ") +
                    sessionBus.lastError().message());
        }
        else
        {
            DBG_APP(QStringLiteral("D-Bus: service com.protonvpn.app registered on session bus"));
        }
    }
    else
    {
        DBG_APP(QStringLiteral("D-Bus: cannot connect to session bus - VPN status will not be exposed"));
    }

    // Starting hidden is only safe when a tray icon exists to restore the
    // window from; otherwise the app would be running with no way to reach it.
    if (AppConfig::instance().startHidden() && QSystemTrayIcon::isSystemTrayAvailable())
    {
        w.hide();
    }
    else
    {
        if (AppConfig::instance().startHidden())
        {
            DBG_APP(QStringLiteral("start_hidden is set but no system tray is available - "
                                   "showing the window instead."));
        }
        w.show();
    }
    return QApplication::exec();
}

