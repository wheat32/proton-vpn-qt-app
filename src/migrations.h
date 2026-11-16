#pragma once

#include <QString>
#include <QStringList>

// Migrations – one-time tasks that run at startup after an upgrade.
//
// Adding a migration:
//   1. Declare the function here.
//   2. Implement it in migrations.cpp and add a call in run().
//
// Removing a migration:
//   1. Delete the implementation in migrations.cpp.
//   2. Delete the call from run().
//   3. Delete the declaration below.

namespace Migrations
{
    // Master function
    void run(const QString& previousVersion);

    // Replaces the legacy systemd user service with an equivalent XDG autostart
    // .desktop entry, if auto-start was active under the old version.
    void migrateSystemdToXdgAutostart(const QString& previousVersion);

    // Flatpak only: moves the autostart entry out of the sandbox's own config
    // directory, where older builds wrote it and no desktop ever read it, to
    // the host's autostart directory. Not tied to a version: it runs whenever
    // such an entry is found.
    void moveLegacyFlatpakAutostart();

    // The decisions and file steps behind the migrations above, with every
    // path passed in so they can run against a scratch directory. The steps
    // return one line per action taken (or, in a dry run, per action that
    // would be taken), for the log.
    namespace Detail
    {
        // Whether `previousVersion` may have left a systemd unit behind. True
        // for an empty or unreadable version too: versions before 1.9.0 never
        // recorded one, so it does not mean a fresh install.
        [[nodiscard]] bool predatesXdgAutostart(const QString& previousVersion);

        // Writes `entryContents` to `autostartFile` (unless an entry is
        // already there), then removes `serviceFile` and the links
        // `systemctl --user enable` made to it. Nothing is removed if the
        // entry cannot be written. Does nothing without a `serviceFile`.
        [[nodiscard]] QStringList replaceSystemdUnit(const QString& serviceFile,
                                                     const QString& autostartFile,
                                                     const QString& entryContents,
                                                     bool dryRun);

        // Moves `legacyFile` to `autostartFile`. An entry already at
        // `autostartFile` is kept and `legacyFile` is only removed. Does
        // nothing without a `legacyFile`, or when both are the same file.
        [[nodiscard]] QStringList moveAutostartEntry(const QString& legacyFile,
                                                     const QString& autostartFile,
                                                     bool dryRun);
    }

#ifdef QT_DEBUG
    // Debug-only test helpers: simulate a migration dry-run and return a
    // human-readable log of every condition checked and action that would fire.
    QString testMigrateSystemdToXdgAutostart(const QString& simulatedPreviousVersion);
    QString testMoveLegacyFlatpakAutostart();
    QString testAll(const QString& simulatedPreviousVersion);
#endif
}
