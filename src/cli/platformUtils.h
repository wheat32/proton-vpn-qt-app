#pragma once
// platformUtils.h
// Utilities for identifying the current packaging/runtime environment.

#include "appImageUtils.h"
#include "flatpakUtils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLatin1Char>
#include <QStandardPaths>
#include <QString>

// Returns a human-readable string identifying the current packaging format:
// "Flatpak", "AppImage", or "System".
inline QString packageTypeName()
{
    if (isRunningAsFlatpak())
    {
        return QStringLiteral("Flatpak");
    }
    if (isRunningAsAppImage())
    {
        return QStringLiteral("AppImage");
    }
    return QStringLiteral("System");
}

namespace PlatformUtils
{
namespace Detail
{
// Quotes a program path for a .desktop file's Exec= key.
//
// The Desktop Entry spec requires arguments containing spaces to be enclosed in
// double quotes, with a backslash escape for the reserved characters. Without
// this, an install path such as /home/me/My Apps/ProtonVPN.AppImage is parsed
// as two separate arguments and the entry never launches.
inline QString quoteExecArgument(const QString& value)
{
    if (value.contains(QLatin1Char(' ')) == false &&
        value.contains(QLatin1Char('"')) == false &&
        value.contains(QLatin1Char('\\')) == false &&
        value.contains(QLatin1Char('$')) == false &&
        value.contains(QLatin1Char('`')) == false)
    {
        return value;
    }

    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'),  QStringLiteral("\\\""));
    escaped.replace(QLatin1Char('$'),  QStringLiteral("\\$"));
    escaped.replace(QLatin1Char('`'),  QStringLiteral("\\`"));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}
// Name of the app's XDG autostart entry.
inline const QString AUTOSTART_FILE_NAME = QStringLiteral("proton-vpn-qt.desktop");

// Where XDG autostart entries go, given the pieces autostartDir() reads from
// the environment.
//
// In the Flatpak the config location is the sandbox's own
// (~/.var/app/<id>/config), which no desktop reads autostart entries from. The
// manifest grants ~/.config/autostart on the host for this, so that is the
// directory to write.
inline QString autostartDirFor(const bool flatpak, const QString& homeDir, const QString& configDir)
{
    if (flatpak == true)
    {
        return homeDir + QStringLiteral("/.config/autostart");
    }
    return configDir + QStringLiteral("/autostart");
}

// Where the single-instance lock goes, given the pieces
// singleInstanceLockPath() reads from the environment.
//
// The lock is per user: in the shared temp directory, a second user on the
// same machine found the first user's lock and was told the app was already
// running. The runtime directory (XDG_RUNTIME_DIR) belongs to one user. In the
// Flatpak that directory is private to each sandbox, and `appRuntimeDir` (the
// one Flatpak shares between an app's instances) is used when it exists. The
// temp directory is the fallback where no runtime directory is set.
inline QString singleInstanceLockPathFor(const QString& runtimeDir, const QString& appRuntimeDir,
                                         const QString& tempDir)
{
    const QString fileName = QStringLiteral("/proton-vpn-qt-app.lock");
    if (appRuntimeDir.isEmpty() == false)
    {
        return appRuntimeDir + fileName;
    }
    if (runtimeDir.isEmpty() == false)
    {
        return runtimeDir + fileName;
    }
    return tempDir + fileName;
}
} // namespace Detail

// Directory the app's XDG autostart entry is written to.
inline QString autostartDir()
{
    return Detail::autostartDirFor(isRunningAsFlatpak(), QDir::homePath(),
                                   QStandardPaths::writableLocation(QStandardPaths::ConfigLocation));
}

// Full path of the app's XDG autostart entry.
inline QString autostartFilePath()
{
    return autostartDir() + QLatin1Char('/') + Detail::AUTOSTART_FILE_NAME;
}

// Where the Flatpak wrote the entry before it used autostartDir(): inside the
// sandbox's own config directory, where it never took effect. Only for moving
// such an entry to the right place.
inline QString legacyFlatpakAutostartFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/autostart/") + Detail::AUTOSTART_FILE_NAME;
}

// Path of the lock file that keeps the app to one running copy per user.
inline QString singleInstanceLockPath()
{
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    QString appRuntimeDir;
    if (isRunningAsFlatpak() == true && runtimeDir.isEmpty() == false)
    {
        const QString shared = runtimeDir + QStringLiteral("/app/") + qEnvironmentVariable("FLATPAK_ID");
        if (QFileInfo(shared).isDir() == true)
        {
            appRuntimeDir = shared;
        }
    }
    return Detail::singleInstanceLockPathFor(runtimeDir, appRuntimeDir, QDir::tempPath());
}

// Returns the command that should go in the Exec= key of a .desktop file so the
// app relaunches correctly on the next login, for every packaging format:
//
//   Flatpak  : "flatpak run <app-id>"        (the binary is inside the sandbox)
//   AppImage : the path of the .AppImage     ($APPIMAGE - see appImagePath())
//   System   : the installed executable path
inline QString autostartExecCommand()
{
    if (isRunningAsFlatpak())
    {
        return QStringLiteral("flatpak run ") + qEnvironmentVariable("FLATPAK_ID");
    }

    if (isRunningAsAppImage())
    {
        const QString path = appImagePath();
        if (path.isEmpty() == false)
        {
            return Detail::quoteExecArgument(path);
        }
    }

    return Detail::quoteExecArgument(QCoreApplication::applicationFilePath());
}
} // namespace PlatformUtils
