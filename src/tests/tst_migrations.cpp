#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "migrations.h"

// The file steps behind the startup migrations, run against a scratch
// directory. A migration that goes wrong does so silently and only once, on
// someone else's machine, so every branch that removes or keeps a file is
// pinned down here.

class TstMigrations : public QObject
{
    Q_OBJECT

private:
    static void writeFile(const QString& path, const QByteArray& contents = "x")
    {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    static QByteArray readFile(const QString& path)
    {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly) == false)
        {
            return QByteArray();
        }
        return file.readAll();
    }

private slots:
    //  predatesXdgAutostart

    void predatesXdgAutostart_emptyOrUnreadableVersion_isTrue()
    {
        // Versions before 1.9.0 never recorded the last version seen, so an
        // upgrade from one looks like this.
        QVERIFY(Migrations::Detail::predatesXdgAutostart(QString()));
        QVERIFY(Migrations::Detail::predatesXdgAutostart(QStringLiteral("garbage")));
    }

    void predatesXdgAutostart_upToLastSystemdVersion_isTrue()
    {
        QVERIFY(Migrations::Detail::predatesXdgAutostart(QStringLiteral("1.8.5")));
        QVERIFY(Migrations::Detail::predatesXdgAutostart(QStringLiteral("1.9.0")));
    }

    void predatesXdgAutostart_laterVersions_isFalse()
    {
        QCOMPARE(Migrations::Detail::predatesXdgAutostart(QStringLiteral("1.9.1")), false);
        QCOMPARE(Migrations::Detail::predatesXdgAutostart(QStringLiteral("1.13.1")), false);
    }

    //  replaceSystemdUnit

    void replaceSystemdUnit_noUnit_doesNothing()
    {
        QTemporaryDir dir;
        const QString entry = dir.filePath(QStringLiteral("autostart/app.desktop"));

        const QStringList log = Migrations::Detail::replaceSystemdUnit(
            dir.filePath(QStringLiteral("systemd/user/app.service")), entry, QStringLiteral("[Desktop Entry]"), false);

        QVERIFY(log.isEmpty());
        QCOMPARE(QFileInfo::exists(entry), false);
    }

    void replaceSystemdUnit_unitAndLink_writesEntryAndRemovesBoth()
    {
        QTemporaryDir dir;
        const QString unit = dir.filePath(QStringLiteral("systemd/user/app.service"));
        const QString link = dir.filePath(QStringLiteral("systemd/user/graphical-session.target.wants/app.service"));
        const QString entry = dir.filePath(QStringLiteral("autostart/app.desktop"));
        writeFile(unit);
        QVERIFY(QDir().mkpath(QFileInfo(link).absolutePath()));
        QVERIFY(QFile::link(unit, link));

        const QStringList log = Migrations::Detail::replaceSystemdUnit(unit, entry, QStringLiteral("[Desktop Entry]"), false);

        QVERIFY(log.isEmpty() == false);
        QCOMPARE(readFile(entry), QByteArray("[Desktop Entry]"));
        QCOMPARE(QFileInfo::exists(unit), false);
        QCOMPARE(QFileInfo(link).isSymLink(), false);
    }

    void replaceSystemdUnit_existingEntry_isKept()
    {
        QTemporaryDir dir;
        const QString unit = dir.filePath(QStringLiteral("systemd/user/app.service"));
        const QString entry = dir.filePath(QStringLiteral("autostart/app.desktop"));
        writeFile(unit);
        writeFile(entry, "the user's own entry");

        (void)Migrations::Detail::replaceSystemdUnit(unit, entry, QStringLiteral("[Desktop Entry]"), false);

        QCOMPARE(readFile(entry), QByteArray("the user's own entry"));
        QCOMPARE(QFileInfo::exists(unit), false);
    }

    void replaceSystemdUnit_noEntryContents_leavesUnitInPlace()
    {
        // Without the template there is nothing to replace the unit with, and
        // removing it would end auto-start.
        QTemporaryDir dir;
        const QString unit = dir.filePath(QStringLiteral("systemd/user/app.service"));
        const QString entry = dir.filePath(QStringLiteral("autostart/app.desktop"));
        writeFile(unit);

        (void)Migrations::Detail::replaceSystemdUnit(unit, entry, QString(), false);

        QVERIFY(QFileInfo::exists(unit));
        QCOMPARE(QFileInfo::exists(entry), false);
    }

    void replaceSystemdUnit_entryCannotBeWritten_leavesUnitInPlace()
    {
        QTemporaryDir dir;
        const QString unit = dir.filePath(QStringLiteral("systemd/user/app.service"));
        writeFile(unit);
        // A file where the autostart directory would have to be created.
        writeFile(dir.filePath(QStringLiteral("autostart")));
        const QString entry = dir.filePath(QStringLiteral("autostart/app.desktop"));

        (void)Migrations::Detail::replaceSystemdUnit(unit, entry, QStringLiteral("[Desktop Entry]"), false);

        QVERIFY(QFileInfo::exists(unit));
    }

    void replaceSystemdUnit_linkToAnotherUnit_isLeftAlone()
    {
        QTemporaryDir dir;
        const QString unit = dir.filePath(QStringLiteral("systemd/user/app.service"));
        const QString other = dir.filePath(QStringLiteral("elsewhere/app.service"));
        const QString link = dir.filePath(QStringLiteral("systemd/user/default.target.wants/app.service"));
        const QString entry = dir.filePath(QStringLiteral("autostart/app.desktop"));
        writeFile(unit);
        writeFile(other);
        QVERIFY(QDir().mkpath(QFileInfo(link).absolutePath()));
        QVERIFY(QFile::link(other, link));

        (void)Migrations::Detail::replaceSystemdUnit(unit, entry, QStringLiteral("[Desktop Entry]"), false);

        QVERIFY(QFileInfo(link).isSymLink());
        QCOMPARE(QFileInfo::exists(unit), false);
    }

    void replaceSystemdUnit_dryRun_changesNothing()
    {
        QTemporaryDir dir;
        const QString unit = dir.filePath(QStringLiteral("systemd/user/app.service"));
        const QString entry = dir.filePath(QStringLiteral("autostart/app.desktop"));
        writeFile(unit);

        const QStringList log = Migrations::Detail::replaceSystemdUnit(unit, entry, QStringLiteral("[Desktop Entry]"), true);

        QVERIFY(log.isEmpty() == false);
        QVERIFY(QFileInfo::exists(unit));
        QCOMPARE(QFileInfo::exists(entry), false);
    }

    //  moveAutostartEntry

    void moveAutostartEntry_noLegacyFile_doesNothing()
    {
        QTemporaryDir dir;
        const QString target = dir.filePath(QStringLiteral("host/autostart/app.desktop"));

        const QStringList log = Migrations::Detail::moveAutostartEntry(
            dir.filePath(QStringLiteral("sandbox/autostart/app.desktop")), target, false);

        QVERIFY(log.isEmpty());
        QCOMPARE(QFileInfo::exists(target), false);
    }

    void moveAutostartEntry_legacyFile_movesIt()
    {
        QTemporaryDir dir;
        const QString legacy = dir.filePath(QStringLiteral("sandbox/autostart/app.desktop"));
        const QString target = dir.filePath(QStringLiteral("host/autostart/app.desktop"));
        writeFile(legacy, "entry");

        (void)Migrations::Detail::moveAutostartEntry(legacy, target, false);

        QCOMPARE(readFile(target), QByteArray("entry"));
        QCOMPARE(QFileInfo::exists(legacy), false);
    }

    void moveAutostartEntry_targetExists_keepsItAndRemovesLegacy()
    {
        QTemporaryDir dir;
        const QString legacy = dir.filePath(QStringLiteral("sandbox/autostart/app.desktop"));
        const QString target = dir.filePath(QStringLiteral("host/autostart/app.desktop"));
        writeFile(legacy, "old");
        writeFile(target, "current");

        (void)Migrations::Detail::moveAutostartEntry(legacy, target, false);

        QCOMPARE(readFile(target), QByteArray("current"));
        QCOMPARE(QFileInfo::exists(legacy), false);
    }

    void moveAutostartEntry_samePath_keepsTheFile()
    {
        // Outside the Flatpak both paths are the same file, which must not be
        // "moved" out of existence.
        QTemporaryDir dir;
        const QString entry = dir.filePath(QStringLiteral("autostart/app.desktop"));
        writeFile(entry, "entry");

        (void)Migrations::Detail::moveAutostartEntry(entry, entry, false);

        QCOMPARE(readFile(entry), QByteArray("entry"));
    }

    void moveAutostartEntry_dryRun_changesNothing()
    {
        QTemporaryDir dir;
        const QString legacy = dir.filePath(QStringLiteral("sandbox/autostart/app.desktop"));
        const QString target = dir.filePath(QStringLiteral("host/autostart/app.desktop"));
        writeFile(legacy, "entry");

        const QStringList log = Migrations::Detail::moveAutostartEntry(legacy, target, true);

        QVERIFY(log.isEmpty() == false);
        QVERIFY(QFileInfo::exists(legacy));
        QCOMPARE(QFileInfo::exists(target), false);
    }
};

QTEST_MAIN(TstMigrations)
#include "tst_migrations.moc"
