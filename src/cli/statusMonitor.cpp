// statusMonitor.cpp
// StatusMonitor: long-lived subprocess that runs `protonvpn status` in a
// loop.  See statusMonitor.h for the full description.

#include "statusMonitor.h"
#include "cliNoiseFilter.h"
#include "flatpakUtils.h"
#include "../debug.h"

#include <QTimer>
#include <csignal>
#include <sys/prctl.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

namespace
{
// The process will appear under this name in ps/top/htop/bpftrace/journalctl.
// Linux TASK_COMM_LEN is 16 bytes (15 usable chars); the full name is always
// visible in /proc/PID/cmdline and `ps aux`.
constexpr char PROCESS_NAME[] = "protonvpn-qt-status-mon"; // NOLINT(*-avoid-c-arrays)

// How long to wait before restarting the monitor after an unexpected exit.
constexpr int RESTART_DELAY_MS = 5'000;

// How long to wait for the subprocess to report that it started.
constexpr int PROCESS_START_TIMEOUT_MS = 2000;

// How long stop() waits for the killed monitor process to be reaped.
// SIGKILL ends it immediately; this is only an upper bound.
constexpr int KILL_REAP_TIMEOUT_MS = 1000;

// Phrases that mean "there is no active connection" on CLI versions that print
// a sentence instead of a "Status:" line. Recognizing them lets an otherwise
// key-less snapshot still count as an authoritative Disconnected, so a genuinely
// unparseable snapshot (a traceback, a hang) stays distinguishable from one.
constexpr const char* NO_CONNECTION_PHRASES[] = {
    "no active",
    "not connected",
    "no connection",
};

// What the loop sleeps for when it does not poll periodically.
const QString SLEEP_FOREVER = QStringLiteral("infinity");

// Between the server name and its location in a `protonvpn status` server
// string: "US-NJ#203 in Secaucus, United States".
const QString SERVER_LOCATION_SEPARATOR = QStringLiteral(" in ");

// The countries Secure Core connections enter through. A server named
// "<entry>-<exit>#n" with one of these first is a Secure Core server.
const QStringList SECURE_CORE_ENTRY_COUNTRIES = {
    QStringLiteral("CH"), QStringLiteral("IS"), QStringLiteral("SE")};

// Length of a country code, as in "CH-US#1".
constexpr int COUNTRY_CODE_LENGTH = 2;
} // namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

StatusMonitor::StatusMonitor(QObject* parent)
    : QObject(parent)
{
    // Owned restart timer rather than a fire-and-forget singleShot: stop() has
    // to be able to cancel a restart that is already scheduled, otherwise a
    // stop() followed by a start() inside the delay window launches a second
    // monitor process that nothing holds a handle to.
    m_restartTimer = new QTimer(this);
    m_restartTimer->setSingleShot(true);
    m_restartTimer->setInterval(RESTART_DELAY_MS);
    connect(m_restartTimer, &QTimer::timeout, this, &StatusMonitor::launchProcess);
}

StatusMonitor::~StatusMonitor()
{
    stop();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void StatusMonitor::setPollInterval(const int seconds)
{
    const int interval = seconds > 0 ? seconds : NO_PERIODIC_POLL;
    if (m_pollSeconds == interval) return;
    m_pollSeconds = interval;
    restart(); // no-op unless running
}

// static
// The shell command run by the subprocess:
//   1. exec -a renames the bash process to PROCESS_NAME (sets argv[0]).
//   2. The inner bash loops: the status command (stdout + stderr merged),
//      ASCII 0x1E (Record Separator) as the snapshot delimiter, then the sleep.
QString StatusMonitor::loopCommand(const QString& statusCommand, const int intervalSeconds)
{
    const QString sleepFor = intervalSeconds > 0 ? QString::number(intervalSeconds) : SLEEP_FOREVER;
    return QStringLiteral("exec -a %1 /bin/bash -c "
                          "'while true; "
                          "do %2 2>&1; "
                          "printf \"\\x1e\"; "
                          "sleep %3; "
                          "done'")
        .arg(QString::fromLatin1(PROCESS_NAME), statusCommand, sleepFor);
}

void StatusMonitor::start()
{
    if (m_process != nullptr)
    {
        DBG_STATUS(QStringLiteral("start() called but monitor is already running "
                               "(PID %1) - ignored.").arg(m_process->processId()));
        return;
    }

    m_stopping = false;
    launchProcess();
}

void StatusMonitor::stop()
{
    m_stopping = true;
    m_restartTimer->stop();

    if (m_process == nullptr) return;

    DBG_STATUS(QStringLiteral("Stopping (PID %1).").arg(m_process->processId()));

    // Disconnect finished() so onProcessFinished() does not schedule an
    // auto-restart after we deliberately kill the process.
    disconnect(m_process, nullptr, this, nullptr);
    m_process->kill();
    // Collect the killed process before deleting the QProcess. Deleting one
    // whose child has not been reaped makes Qt warn "Destroyed while process
    // is still running" and block in the destructor until it has been.
    m_process->waitForFinished(KILL_REAP_TIMEOUT_MS);
    m_process->deleteLater();
    m_process = nullptr;
    m_buffer.clear();
}

void StatusMonitor::restart()
{
    // Not running and not waiting to restart either: never started, or stopped.
    if (m_process == nullptr && m_restartTimer->isActive() == false) return;

    DBG_STATUS(QStringLiteral("Restarting for a fresh snapshot."));
    stop();
    start();
}

bool StatusMonitor::isRunning() const
{
    return m_process != nullptr && m_process->state() == QProcess::Running;
}

// ---------------------------------------------------------------------------
// Private - process lifecycle
// ---------------------------------------------------------------------------

void StatusMonitor::launchProcess()
{
    if (m_stopping) return;
    if (m_process != nullptr) return; // already running - never run two monitors at once

    m_buffer.clear();
    m_process = new QProcess(this);

    // Ask the kernel to deliver SIGTERM to this child if the parent process
    // dies for any reason, including SIGKILL.  The lambda runs in the child
    // after fork() but before exec(), which is the only safe place to call
    // prctl().  This prevents protonvpn-qt-status-mon from becoming an orphan
    // if the GUI process is hard-killed or crashes.
    m_process->setChildProcessModifier([]
    {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
    });

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &StatusMonitor::onReadyRead);

    connect(m_process,
            &QProcess::finished,
            this, &StatusMonitor::onProcessFinished);

    // Inside a Flatpak sandbox, `protonvpn` is not available directly - it
    // must be forwarded to the host via flatpak-spawn.
    const QString statusCommand = isRunningAsFlatpak()
        ? QStringLiteral("flatpak-spawn --host protonvpn status")
        : QStringLiteral("protonvpn status");
    const QString command = loopCommand(statusCommand, m_pollSeconds);
    m_process->start(QStringLiteral("/bin/bash"),
                     {QStringLiteral("-c"), command});

    if (m_process->waitForStarted(PROCESS_START_TIMEOUT_MS))
    {
        DBG_STATUS(QStringLiteral("Process launched:"
                               "  name=\"%1\"  PID=%2  restart#=%3"
                               "  command: /bin/bash -c \"%4\"")
                    .arg(QString::fromLatin1(PROCESS_NAME))
                    .arg(m_process->processId())
                    .arg(m_restartCount)
                    .arg(command));
    }
    else
    {
        DBG_STATUS(QStringLiteral("ERROR: process failed to start within %1 ms "
                               "(QProcess::ProcessError=%2).")
                    .arg(PROCESS_START_TIMEOUT_MS).arg(m_process->error()));
    }
}

void StatusMonitor::onReadyRead()
{
    const QString chunk = QString::fromUtf8(m_process->readAllStandardOutput());

    DBG_STATUS(QStringLiteral("Raw data received (%1 bytes).").arg(chunk.size()));

    m_buffer += chunk;

    // Each complete `protonvpn status` snapshot is terminated by ASCII 0x1E
    // (Record Separator).  Accumulate until we see the delimiter.
    int sepPos;
    while ((sepPos = m_buffer.indexOf(QLatin1Char('\x1e'))) != -1)
    {
        const QString snapshot = m_buffer.left(sepPos);
        m_buffer.remove(0, sepPos + 1);

        const QMap<QString, QString> fields = parseStatusFields(snapshot);

        DBG_STATUS(QStringLiteral("Snapshot parsed:"
                               "  status=\"%1\"  server=\"%2\"  fields=%3.")
                    .arg(fields.value(QStringLiteral("status")),
                         fields.value(QStringLiteral("server")))
                    .arg(fields.size()));

        emit statusParsed(fields);
    }
}

void StatusMonitor::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    // CrashExit means the process was terminated by a signal - either an
    // external `kill`/`kill -9` or the kernel via PR_SET_PDEATHSIG.
    // NormalExit with a non-zero code means the shell loop exited on its own.
    if (status == QProcess::CrashExit)
    {
        DBG_STATUS(QStringLiteral("Process was killed externally (signal termination)."
                               "  restart#=%1 - restarting in %2 ms.")
                    .arg(m_restartCount).arg(RESTART_DELAY_MS));
    }
    else
    {
        DBG_STATUS(QStringLiteral("Process exited unexpectedly (non-zero exit):"
                               "  exitCode=%1  restart#=%2 - restarting in %3 ms.")
                    .arg(exitCode).arg(m_restartCount).arg(RESTART_DELAY_MS));
    }

    m_process->deleteLater();
    m_process = nullptr;
    m_buffer.clear();
    ++m_restartCount;

    m_restartTimer->start();
}

// ---------------------------------------------------------------------------
// Static parsing helpers
// ---------------------------------------------------------------------------

// static
QMap<QString, QString> StatusMonitor::parseStatusFields(const QString& combined)
{
    // Remove noise / informational lines that are not "Key: Value" data.
    const QStringList lines = CliNoise::strip(combined.split(QLatin1Char('\n')));

    QMap<QString, QString> fields;
    for (const QString& line : lines)
    {
        const int colonPos = line.indexOf(QLatin1Char(':'));
        if (colonPos < 0) continue;
        const QString key   = line.left(colonPos).trimmed().toLower();
        const QString value = line.mid(colonPos + 1).trimmed();
        if (key.isEmpty() == false && value.isEmpty() == false)
        {
            fields.insert(key, value);
        }
    }

    // No "Status:" line, but the output says in prose that nothing is
    // connected: synthesize the field so callers still get an authoritative
    // answer. Without this the snapshot would be indistinguishable from a CLI
    // failure, which callers must ignore rather than read as "disconnected".
    if (fields.contains(QStringLiteral("status")) == false)
    {
        const QString lowered = combined.toLower();
        for (const char* phrase : NO_CONNECTION_PHRASES)
        {
            if (lowered.contains(QLatin1String(phrase)))
            {
                fields.insert(QStringLiteral("status"), QStringLiteral("Disconnected"));
                break;
            }
        }
    }

    return fields;
}

// static
QString StatusMonitor::parseCityFromServer(const QString& server)
{
    const int inPos = server.indexOf(SERVER_LOCATION_SEPARATOR);
    if (inPos < 0)
    {
        return {};
    }
    const QString rest    = server.mid(inPos + SERVER_LOCATION_SEPARATOR.size());
    const int    commaPos = rest.indexOf(QLatin1Char(','));
    return (commaPos >= 0 ? rest.left(commaPos) : rest).trimmed();
}

// static
QString StatusMonitor::parseServerName(const QString& server)
{
    const int inPos = server.indexOf(SERVER_LOCATION_SEPARATOR);
    return (inPos >= 0 ? server.left(inPos) : server).trimmed();
}

// static
QString StatusMonitor::parseCountryFromServer(const QString& server)
{
    // The name starts with a country code, which runs up to the first '-' or
    // '#', whichever comes first: "DE#42", "US-NJ#203", "JP-FREE#1".
    const QString name = parseServerName(server).toUpper();
    const int dashPos = name.indexOf(QLatin1Char('-'));
    const int hashPos = name.indexOf(QLatin1Char('#'));
    const bool dashFirst = dashPos >= 0 && (hashPos < 0 || dashPos < hashPos);
    const int endPos = dashFirst == true ? dashPos : hashPos;
    if (endPos <= 0)
    {
        return QString();
    }
    const QString first = name.left(endPos);

    // What follows the dash is a US state ("US-NJ#203"), a tag ("JP-FREE#1"),
    // or, for a Secure Core server ("CH-US#1"), the country the connection
    // comes out in. That one is the server's country: the first code is only
    // where the traffic enters.
    if (dashFirst == true && hashPos > dashPos && SECURE_CORE_ENTRY_COUNTRIES.contains(first) == true)
    {
        const QString second = name.mid(dashPos + 1, hashPos - dashPos - 1);
        if (second.size() == COUNTRY_CODE_LENGTH)
        {
            return second;
        }
    }
    return first;
}
