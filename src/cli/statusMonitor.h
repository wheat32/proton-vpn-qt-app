// StatusMonitor: long-lived subprocess that runs `protonvpn status` in a
// loop (every 30 seconds unless set otherwise) and emits a parsed snapshot
// for every iteration.
//
// The subprocess renames itself to "protonvpn-qt-status-mon" via `exec -a`
// so it is easy to identify in ps/top/htop/bpftrace/journalctl traces.
//
// VpnManager owns one instance and connects to statusParsed() to apply state
// changes.  The static helpers parseStatusFields() and the parse*FromServer()
// / parseServerName() family are shared utilities for parsing `protonvpn
// status` output.
#pragma once

#include <QMap>
#include <QProcess>
#include <QString>

class QTimer;

class StatusMonitor : public QObject
{
    Q_OBJECT

public:
    explicit StatusMonitor(QObject* parent = nullptr);
    ~StatusMonitor() override;

    // Seconds between two `protonvpn status` runs. NO_PERIODIC_POLL runs it
    // once per start() or restart() and then waits. Takes effect right away
    // when the monitor is running.
    void setPollInterval(int seconds);
    static constexpr int NO_PERIODIC_POLL    = 0;
    static constexpr int DEFAULT_POLL_SECONDS = 30;

    // The shell loop the monitor runs: `statusCommand`, a record separator,
    // then a sleep of `intervalSeconds` (forever for NO_PERIODIC_POLL).
    static QString loopCommand(const QString& statusCommand, int intervalSeconds);

    // Start the monitor loop.  Safe to call multiple times - ignored if
    // already running.
    void start();

    // Stop the monitor loop and kill the subprocess. Any pending auto-restart
    // timer is also cancelled.
    void stop();

    // Drops the poll that is in flight and starts over with a fresh one,
    // right away instead of at the next interval. Does nothing when the
    // monitor was never started or has been stopped.
    void restart();

    bool isRunning() const;

    //  Shared parsing helpers

    // Strip noise lines from `protonvpn status` output and parse all
    // "Key: Value" pairs into a map (keys are lowercased).
    //
    // A missing "status" key means the snapshot could not be understood (an
    // empty read, a traceback, a hung CLI). Callers must treat that as "no new
    // information" rather than as a disconnect - see
    // VpnManager::applyStatusFields().
    static QMap<QString, QString> parseStatusFields(const QString& combined);

    // Extract the city from a server string like "US-NJ#203 in Secaucus, United States".
    // Returns an empty string if the pattern is not present.
    static QString parseCityFromServer(const QString& server);

    // The server's name from the same string ("US-NJ#203"), as
    // `protonvpn connect` takes it to connect to that exact server.
    static QString parseServerName(const QString& server);

    // The server's country code: the one its name starts with ("US" from
    // "US-NJ#203", "DE" from "DE#42"), or for a Secure Core server the exit
    // country ("US" from "CH-US#1"). Empty when the name has no such prefix.
    static QString parseCountryFromServer(const QString& server);

signals:
    // Emitted once per complete `protonvpn status` snapshot.
    void statusParsed(const QMap<QString, QString>& fields);

private slots:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void launchProcess();

    QProcess* m_process      = nullptr;
    QTimer*   m_restartTimer = nullptr; // owned; canceled by stop()
    QString   m_buffer;
    int       m_pollSeconds  = DEFAULT_POLL_SECONDS;
    int       m_restartCount = 0;
    bool      m_stopping     = false; // set by stop() to suppress auto-restart
};

