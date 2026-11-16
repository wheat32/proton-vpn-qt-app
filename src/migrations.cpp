#include "migrations.h"
#include "cli/platformUtils.h"
#include "debug.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVersionNumber>

namespace
{
#ifdef QT_DEBUG
constexpr bool DRY_RUN_MODE = true;
#else
constexpr bool DRY_RUN_MODE = false;
#endif

// The last version that used a systemd user service for auto-start.
// Versions after this switched to an XDG autostart .desktop file.
const QVersionNumber LAST_SYSTEMD_VERSION = QVersionNumber::fromString(QStringLiteral("1.9.0"));

// The legacy unit, and the directories `systemctl --user enable` linked it
// from (graphical-session.target.wants for the unit this app installed).
const QString SYSTEMD_UNIT_NAME        = QStringLiteral("proton-vpn-qt.service");
const QString SYSTEMD_WANTS_DIR_FILTER = QStringLiteral("*.wants");

const QString LOG_PREFIX     = QStringLiteral("Migration: ");
const QString DRY_RUN_PREFIX = QStringLiteral("[DRY RUN] ");

QString systemdServiceFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/systemd/user/") + SYSTEMD_UNIT_NAME;
}

// The XDG autostart entry for this install, from the bundled template. Empty
// when the template cannot be read.
QString autostartEntryContents()
{
    QFile templateFile(QStringLiteral(":/autostart/proton-vpn-qt.desktop"));
    if (templateFile.open(QIODevice::ReadOnly) == false)
    {
        return QString();
    }
    QString content = QString::fromUtf8(templateFile.readAll());
    content.replace(QStringLiteral("@EXEC@"), PlatformUtils::autostartExecCommand());
    return content;
}

// Writes `contents` to `path`, creating its directory. All or nothing: a
// failure leaves no partial file behind. Returns why it failed, or an empty
// string on success.
QString writeTextFile(const QString& path, const QString& contents)
{
    const QString dirPath = QFileInfo(path).absolutePath();
    if (QDir().mkpath(dirPath) == false)
    {
        return QStringLiteral("could not create ") + dirPath;
    }

    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly) == false)
    {
        return file.errorString();
    }
    file.write(contents.toUtf8());
    if (file.commit() == false)
    {
        return file.errorString();
    }
    return QString();
}

// The links to `serviceFile` in the *.wants directories beside it, which is
// where `systemctl --user enable` put them. A link by the unit's name that
// already points nowhere counts too; one that points at some other file is
// not ours and is left alone.
QStringList systemdWantsLinks(const QString& serviceFile)
{
    const QFileInfo unit(serviceFile);
    const QString unitTarget = unit.canonicalFilePath();

    QStringList links;
    const QFileInfoList wantsDirs = unit.absoluteDir().entryInfoList(
        QStringList() << SYSTEMD_WANTS_DIR_FILTER, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& wantsDir : wantsDirs)
    {
        const QFileInfo link(wantsDir.absoluteFilePath() + QLatin1Char('/') + unit.fileName());
        if (link.isSymLink() == false) continue;

        const QString target = link.canonicalFilePath(); // empty for a dangling link
        if (target.isEmpty() == true || target == unitTarget)
        {
            links << link.absoluteFilePath();
        }
    }
    return links;
}

void logLines(const QStringList& lines)
{
    for (const QString& line : lines)
    {
        DBG_APP(LOG_PREFIX + line);
    }
}
} // namespace

namespace Migrations
{

void run(const QString& previousVersion)
{
    migrateSystemdToXdgAutostart(previousVersion);
    moveLegacyFlatpakAutostart();
}

void migrateSystemdToXdgAutostart(const QString& previousVersion)
{
    if (Detail::predatesXdgAutostart(previousVersion) == false) return;

    // If the service file does not exist, auto-start was not enabled under the
    // old version (or this is a fresh install), so remove nothing, create
    // nothing.
    const QString serviceFile = systemdServiceFilePath();
    if (QFileInfo::exists(serviceFile) == false) return;

    DBG_APP(LOG_PREFIX + QStringLiteral("replacing legacy systemd unit with XDG autostart entry"));
    logLines(Detail::replaceSystemdUnit(serviceFile, PlatformUtils::autostartFilePath(),
                                        autostartEntryContents(), DRY_RUN_MODE));
}

void moveLegacyFlatpakAutostart()
{
    // Only the Flatpak wrote the entry to the wrong place. Everywhere else the
    // legacy path is the right one, and the entry there must stay.
    if (isRunningAsFlatpak() == false) return;

    logLines(Detail::moveAutostartEntry(PlatformUtils::legacyFlatpakAutostartFilePath(),
                                        PlatformUtils::autostartFilePath(), DRY_RUN_MODE));
}

namespace Detail
{

bool predatesXdgAutostart(const QString& previousVersion)
{
    // A version that cannot be read is not past the migration point. Versions
    // before 1.9.0 never recorded the last version seen, so an upgrade from
    // one arrives here with an empty string, exactly like a fresh install;
    // whether the service file exists tells the two apart.
    const QVersionNumber previous = QVersionNumber::fromString(previousVersion);
    return previous.isNull() == true || previous <= LAST_SYSTEMD_VERSION;
}

QStringList replaceSystemdUnit(const QString& serviceFile, const QString& autostartFile,
                               const QString& entryContents, const bool dryRun)
{
    QStringList log;
    if (QFileInfo::exists(serviceFile) == false)
    {
        return log;
    }

    // The new entry comes first. If it cannot be written the unit stays as it
    // is, so a failure never leaves the user without auto-start.
    if (QFileInfo::exists(autostartFile) == true)
    {
        log << QStringLiteral("An XDG autostart entry already exists, keeping it: ") + autostartFile;
    }
    else if (entryContents.isEmpty() == true)
    {
        log << QStringLiteral("Could not read the autostart template; leaving the systemd unit in place.");
        return log;
    }
    else if (dryRun == true)
    {
        log << DRY_RUN_PREFIX + QStringLiteral("Would write XDG autostart entry: ") + autostartFile;
    }
    else
    {
        const QString error = writeTextFile(autostartFile, entryContents);
        if (error.isEmpty() == false)
        {
            log << QStringLiteral("Could not write the XDG autostart entry %1 (%2); "
                                  "leaving the systemd unit in place.").arg(autostartFile, error);
            return log;
        }
        log << QStringLiteral("Wrote XDG autostart entry: ") + autostartFile;
    }

    // Found before the unit goes: once it is gone, a link to it can no longer
    // be told from any other dangling link by what it points at.
    const QStringList links = systemdWantsLinks(serviceFile);

    if (dryRun == true)
    {
        log << DRY_RUN_PREFIX + QStringLiteral("Would remove: ") + serviceFile;
        for (const QString& link : links)
        {
            log << DRY_RUN_PREFIX + QStringLiteral("Would remove: ") + link;
        }
        return log;
    }

    if (QFile::remove(serviceFile) == true)
    {
        log << QStringLiteral("Removed systemd unit: ") + serviceFile;
    }
    else
    {
        log << QStringLiteral("Could not remove the systemd unit: ") + serviceFile;
    }

    // The links go even if the unit could not be removed: without them the
    // unit no longer starts the app, which would otherwise launch twice.
    for (const QString& link : links)
    {
        if (QFile::remove(link) == true)
        {
            log << QStringLiteral("Removed systemd link: ") + link;
            // As `systemctl --user disable` does; fails harmlessly when the
            // directory still holds other units' links.
            QDir().rmdir(QFileInfo(link).absolutePath());
        }
        else
        {
            log << QStringLiteral("Could not remove the systemd link: ") + link;
        }
    }
    return log;
}

QStringList moveAutostartEntry(const QString& legacyFile, const QString& autostartFile, const bool dryRun)
{
    QStringList log;
    if (QFileInfo::exists(legacyFile) == false)
    {
        return log;
    }

    // One and the same file (outside the Flatpak, or through a link): there
    // is nothing to move, and removing it would turn auto-start off.
    const QFileInfo legacy(legacyFile);
    const QFileInfo target(autostartFile);
    if (legacy.absoluteFilePath() == target.absoluteFilePath() ||
        legacy.canonicalFilePath() == target.canonicalFilePath())
    {
        return log;
    }

    if (target.exists() == true)
    {
        log << QStringLiteral("An autostart entry already exists, keeping it: ") + autostartFile;
    }
    else if (dryRun == true)
    {
        log << DRY_RUN_PREFIX + QStringLiteral("Would move %1 to %2").arg(legacyFile, autostartFile);
    }
    else
    {
        // Copied, then removed below, rather than renamed: the two directories
        // are separate mounts inside the sandbox. A failure leaves the legacy
        // entry where it is, so the next launch tries again.
        if (QDir().mkpath(target.absolutePath()) == false || QFile::copy(legacyFile, autostartFile) == false)
        {
            log << QStringLiteral("Could not copy the autostart entry %1 to %2; leaving it for the next launch.")
                       .arg(legacyFile, autostartFile);
            return log;
        }
        log << QStringLiteral("Moved the autostart entry %1 to %2").arg(legacyFile, autostartFile);
    }

    if (dryRun == true)
    {
        log << DRY_RUN_PREFIX + QStringLiteral("Would remove: ") + legacyFile;
        return log;
    }

    if (QFile::remove(legacyFile) == true)
    {
        log << QStringLiteral("Removed the legacy autostart entry: ") + legacyFile;
    }
    else
    {
        log << QStringLiteral("Could not remove the legacy autostart entry: ") + legacyFile;
    }
    return log;
}

} // namespace Detail

} // namespace Migrations

// ---------------------------------------------------------------------------
// Debug-only test helpers
// ---------------------------------------------------------------------------

#ifdef QT_DEBUG

QString Migrations::testMigrateSystemdToXdgAutostart(const QString& simulatedPreviousVersion)
{
    const QVersionNumber prev = QVersionNumber::fromString(simulatedPreviousVersion);

    QStringList log;
    auto logLine = [&log](const QString& line)
    {
        log << line;
        DBG_APP(line);
    };

    DBG_APP(QStringLiteral("[TEST] migrateSystemdToXdgAutostart"));
    logLine(QStringLiteral("Simulated previous version: '%1'  (parsed: %2)")
            .arg(simulatedPreviousVersion,
                 prev.isNull() ? QStringLiteral("(null/invalid)") : prev.toString()));

    if (Detail::predatesXdgAutostart(simulatedPreviousVersion) == false)
    {
        logLine(QStringLiteral("→ SKIP: %1 > %2 (already past migration point)")
                .arg(prev.toString(), LAST_SYSTEMD_VERSION.toString()));
        return log.join(QLatin1Char('\n'));
    }
    if (prev.isNull() == true)
    {
        logLine(QStringLiteral("No readable version: an upgrade from before 1.9.0 or a fresh install; "
                               "the service file decides."));
    }

    const QString serviceFile = systemdServiceFilePath();
    logLine(QStringLiteral("Checking for service file: %1").arg(serviceFile));

    if (QFileInfo::exists(serviceFile) == false)
    {
        logLine(QStringLiteral("→ SKIP: service file not found (auto-start was not enabled under old version)"));
        return log.join(QLatin1Char('\n'));
    }

    logLine(QStringLiteral("Service file found."));
    // The same steps the real migration takes, as a dry run.
    const QStringList steps = Detail::replaceSystemdUnit(serviceFile, PlatformUtils::autostartFilePath(),
                                                         autostartEntryContents(), true);
    for (const QString& step : steps)
    {
        logLine(QStringLiteral("→ ") + step);
    }

    return log.join(QLatin1Char('\n'));
}

QString Migrations::testMoveLegacyFlatpakAutostart()
{
    QStringList log;
    auto logLine = [&log](const QString& line)
    {
        log << line;
        DBG_APP(line);
    };

    DBG_APP(QStringLiteral("[TEST] moveLegacyFlatpakAutostart"));
    if (isRunningAsFlatpak() == false)
    {
        logLine(QStringLiteral("→ SKIP: not running as a Flatpak (only the Flatpak wrote the entry to the wrong place)"));
        return log.join(QLatin1Char('\n'));
    }

    const QString legacyFile = PlatformUtils::legacyFlatpakAutostartFilePath();
    logLine(QStringLiteral("Checking for legacy entry: %1").arg(legacyFile));

    // The same steps the real migration takes, as a dry run.
    const QStringList steps = Detail::moveAutostartEntry(legacyFile, PlatformUtils::autostartFilePath(), true);
    if (steps.isEmpty() == true)
    {
        logLine(QStringLiteral("→ SKIP: no legacy entry found"));
    }
    for (const QString& step : steps)
    {
        logLine(QStringLiteral("→ ") + step);
    }

    return log.join(QLatin1Char('\n'));
}

QString Migrations::testAll(const QString& simulatedPreviousVersion)
{
    DBG_APP(QStringLiteral("[TEST] Running all migrations with simulated previous version: '%1'")
            .arg(simulatedPreviousVersion));
    QStringList parts;
    parts << (QStringLiteral("=== migrateSystemdToXdgAutostart ===\n")
              + testMigrateSystemdToXdgAutostart(simulatedPreviousVersion));
    parts << (QStringLiteral("=== moveLegacyFlatpakAutostart ===\n")
              + testMoveLegacyFlatpakAutostart());
    return parts.join(QStringLiteral("\n\n"));
}

#endif // QT_DEBUG
