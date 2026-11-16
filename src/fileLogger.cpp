#include "fileLogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

namespace
{
// Current log file size cap. Checked after every write via QFile::size()
// (a cheap fstat-class call), so the check itself costs nothing.
constexpr qint64 MAX_LOG_FILE_SIZE_BYTES = 64LL * 1024 * 1024; // 64 MiB

// When the cap is hit, trim down to LOG_TRIM_KEEP_PERCENT of the cap rather
// than exactly to it. Trimming means reading and rewriting the kept tail of
// the file, which is real I/O - if we only ever trimmed back down to
// MAX_LOG_FILE_SIZE_BYTES, the very next line written would push it back
// over and re-trigger a full rewrite on every single subsequent write.
constexpr int LOG_TRIM_KEEP_PERCENT = 85;
constexpr qint64 LOG_TRIM_TARGET_BYTES = MAX_LOG_FILE_SIZE_BYTES * LOG_TRIM_KEEP_PERCENT / 100;

// QStandardPaths::GenericStateLocation resolves to:
//   - Native install / AppImage: ~/.local/state/ (AppImage is unsandboxed
//     and shares the host's XDG paths, unlike Flatpak)
//   - Flatpak sandbox          : ~/.var/app/io.github.wheat32.ProtonVPNQt/state/
QString logDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation)
           + QStringLiteral("/ProtonVPN-Qt/logs");
}

QString currentTimestampName()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss")) + QStringLiteral(".log");
}

// Matches a rotated log file name, e.g. "2026-06-30_14-23-05-3.log".
// Capture group 1 is the rotation number (1-9).
const QRegularExpression& rotatedSuffixRe()
{
    static const QRegularExpression re(QStringLiteral(R"(-([1-9])\.log$)"));
    return re;
}
} // namespace

FileLogger& FileLogger::instance()
{
    static FileLogger inst;
    return inst;
}

FileLogger::~FileLogger()
{
    closeLogFile();
}

void FileLogger::setEnabled(const bool enabled)
{
    if (m_enabled == enabled) return;

    m_enabled = enabled;
    if (m_enabled)
    {
        openNewLogFile();
    }
    else
    {
        closeLogFile();
    }
}

void FileLogger::write(const QString& line) const
{
    if (m_enabled == false || m_file == nullptr) return;

    QTextStream stream(m_file);
    stream << line << '\n';
    stream.flush();

    trimIfNeeded();
}

void FileLogger::trimIfNeeded() const
{
    if (m_file == nullptr || m_file->size() <= MAX_LOG_FILE_SIZE_BYTES) return;

    const QString path = m_file->fileName();

    // Read the kept tail via a separate handle so m_file's own state isn't
    // disturbed until we are ready to truncate and rewrite it.
    QFile reader(path);
    if (reader.open(QIODevice::ReadOnly) == false) return;

    reader.seek(reader.size() - LOG_TRIM_TARGET_BYTES);
    QByteArray tail = reader.readAll();
    reader.close();

    // Drop a possibly-truncated partial first line so the kept content
    // starts cleanly at a line boundary.
    const int firstNewline = tail.indexOf('\n');
    if (firstNewline >= 0)
    {
        tail.remove(0, firstNewline + 1);
    }

    m_file->close();
    if (m_file->open(QIODevice::WriteOnly | QIODevice::Text) == false) return;
    m_file->write(tail);
    m_file->flush();
}

void FileLogger::openNewLogFile()
{
    closeLogFile();

    const QString dirPath = logDir();
    const QDir dir;
    if (dir.mkpath(dirPath) == false) return;

    const QDir logDirObj(dirPath);
    const QFileInfoList entries = logDirObj.entryInfoList(
        QStringList() << QStringLiteral("*.log"), QDir::Files, QDir::Time | QDir::Reversed);

    // Split into rotated files (keyed by their number) and the current
    // unsuffixed file, if present.
    QMap<int, QString> rotated; // rotation number -> absolute path
    QString currentPath;

    for (const QFileInfo& fi : entries)
    {
        const QString name = fi.fileName();
        const QRegularExpressionMatch m = rotatedSuffixRe().match(name);
        if (m.hasMatch())
        {
            rotated[m.captured(1).toInt()] = fi.absoluteFilePath();
        }
        else if (currentPath.isEmpty())
        {
            currentPath = fi.absoluteFilePath();
        }
    }

    // Oldest slot is full - drop it.
    if (rotated.contains(MAX_ROTATED_LOGS))
    {
        QFile::remove(rotated[MAX_ROTATED_LOGS]);
        rotated.remove(MAX_ROTATED_LOGS);
    }

    // Shift -8 -> -9, -7 -> -8, ..., -1 -> -2. Must go in descending order so
    // a rename never clobbers a file still waiting to be shifted.
    for (int n = MAX_ROTATED_LOGS - 1; n >= 1; --n)
    {
        if (rotated.contains(n) == false) continue;
        const QString& oldPath = rotated[n];
        QString base = oldPath;
        base.chop(QStringLiteral("-%1.log").arg(n).size());
        QFile::rename(oldPath, base + QStringLiteral("-%1.log").arg(n + 1));
    }

    // The current unsuffixed file (if any) becomes "-1".
    if (currentPath.isEmpty() == false)
    {
        QString base = currentPath;
        base.chop(QStringLiteral(".log").size());
        QFile::rename(currentPath, base + QStringLiteral("-1.log"));
    }

    m_file = new QFile(dirPath + QStringLiteral("/") + currentTimestampName());
    if (m_file->open(QIODevice::WriteOnly | QIODevice::Text) == false)
    {
        delete m_file;
        m_file = nullptr;
    }
}

void FileLogger::closeLogFile()
{
    if (m_file != nullptr)
    {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}
